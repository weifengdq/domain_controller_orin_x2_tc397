#include "bsp_spi.h"

#include "xil_exception.h"
#include "xinterrupt_wrap.h"
#include "xspi.h"

typedef struct {
  XSpi instance;
  spi_callback_t callbacks[MAX_SPI_CALLBACKS];
  bool isr_flag[MAX_SPI_CALLBACKS];
  uint32_t status_event[MAX_SPI_CALLBACKS];
  uint32_t size[MAX_SPI_CALLBACKS];
} spi_t;

static spi_t spis[BSP_SPINUM];

int bsp_spi_register_callback(spi_id_t id, spi_callback_t callback) {
  if (id >= BSP_SPINUM) {
    return -1;
  }
  spi_t *spi = &spis[id];
  for (int i = 0; i < MAX_SPI_CALLBACKS; i++) {
    if (spi->callbacks[i] == NULL) {
      spi->callbacks[i] = callback;
      spi->isr_flag[i] = false;
      return 0;
    }
  }
  return -2;
}

static void spi_status_handler(void *CallBackRef, u32 StatusEvent,
                               unsigned int ByteCount) {
  spi_t *spi = (spi_t *)CallBackRef;
  for (int i = 0; i < MAX_SPI_CALLBACKS; i++) {
    if (spi->callbacks[i] != NULL) {
      spi->isr_flag[i] = true;
      spi->status_event[i] = StatusEvent;
      spi->size[i] = ByteCount;
    }
  }
}

int bsp_spi_init(spi_id_t id, uint32_t base_addr) {
  if (id >= BSP_SPINUM) {
    return -1;
  }
  spi_t *spi = &spis[id];
  XSpi_Config *cfg = XSpi_LookupConfig(base_addr);
  if (cfg == NULL) {
    return -2;
  }
  int status = XSpi_CfgInitialize(&spi->instance, cfg, cfg->BaseAddress);
  if (status != XST_SUCCESS) {
    return -3;
  }
  status = XSetupInterruptSystem(
      &spi->instance, (XInterruptHandler)XSpi_InterruptHandler, cfg->IntrId,
      cfg->IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (status != XST_SUCCESS) {
    return -4;
  }
  XSpi_SetStatusHandler(&spi->instance, &spi->instance, spi_status_handler);
  status = XSpi_SetOptions(&spi->instance,
                           XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION);
  if (status != XST_SUCCESS) {
    return -5;
  }
  status = XSpi_SetSlaveSelect(&spi->instance, 0x01);
  if (status != XST_SUCCESS) {
    return -6;
  }
  status = XSpi_Start(&spi->instance);
  if (status != XST_SUCCESS) {
    return -7;
  }

  return 0;
}

int bsp_spi_process(void) {
  for (int i = 0; i < BSP_SPINUM; i++) {
    spi_t *spi = &spis[i];
    for (int j = 0; j < MAX_SPI_CALLBACKS; j++) {
      if (spi->callbacks[j] != NULL && spi->isr_flag[j]) {
        spi->callbacks[j](spi->status_event[j], spi->size[j]);
        spi->isr_flag[j] = false;
      }
    }
  }
  return 0;
}

int bsp_spi_transfer(spi_id_t id, uint8_t *tx_buf, uint8_t *rx_buf,
                     uint32_t size) {
  if (id >= BSP_SPINUM) {
    return -1;
  }
  spi_t *spi = &spis[id];
  return XSpi_Transfer(&spi->instance, tx_buf, rx_buf, size);
}