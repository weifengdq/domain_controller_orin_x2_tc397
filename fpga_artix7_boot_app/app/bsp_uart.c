#include "bsp_uart.h"

#include <stdbool.h>

#include "xil_exception.h"
#include "xinterrupt_wrap.h"
#include "xuartlite.h"

typedef struct {
  XUartLite instance;
  bool running;
  uint8_t *rx_buf;
  uint8_t *tx_buf;
  int rx_size;
  int tx_size;
  int rx_count;
  int tx_count;
  int rx_expected;
  int tx_expected;
  uart_callback_t callbacks[MAX_UART_CALLBACKS];
} uart_t;

static uart_t uart[BSP_UARTNUM];

int bsp_uart_register_rx_callback(uart_id_t id, uart_callback_t callback) {
  if (id >= BSP_UARTNUM) {
    return -1;
  }
  uart_t *u = &uart[id];
  for (int i = 0; i < MAX_UART_CALLBACKS; i++) {
    if (u->callbacks[i] == NULL) {
      u->callbacks[i] = callback;
      return 0;
    }
  }
  return -2;
}

static void uart_rx_isr_handler(void *CallBackRef, unsigned int EventData) {
  uart_t *u = (uart_t *)CallBackRef;
  u->rx_count = EventData;
}

static void uart_tx_isr_handler(void *CallBackRef, unsigned int EventData) {
  uart_t *u = (uart_t *)CallBackRef;
  u->tx_count = EventData;
}

int bsp_uart_init(uart_id_t id, uint32_t base_addr, uint8_t *rx_buf,
                  uint32_t rx_size, uint8_t *tx_buf, uint32_t tx_size) {
  if (id >= BSP_UARTNUM) {
    return -1;
  }
  uart_t *u = &uart[id];
  u->rx_buf = rx_buf;
  u->tx_buf = tx_buf;
  u->rx_size = rx_size;
  u->tx_size = tx_size;
  XUartLite_Config *cfg = XUartLite_LookupConfig(base_addr);
  if (cfg == NULL) {
    return -2;
  }
  int status = XUartLite_Initialize(&u->instance, base_addr);
  if (status != XST_SUCCESS) {
    return -3;
  }
  status = XSetupInterruptSystem(
      &u->instance, (XInterruptHandler)XUartLite_InterruptHandler, cfg->IntrId,
      cfg->IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (status != XST_SUCCESS) {
    return -4;
  }
  XUartLite_SetRecvHandler(&u->instance, uart_rx_isr_handler, &u->instance);
  XUartLite_SetSendHandler(&u->instance, uart_tx_isr_handler, &u->instance);
  XUartLite_EnableInterrupt(&u->instance);
  for (int i = 0; i < MAX_UART_CALLBACKS; i++) {
    u->callbacks[i] = NULL;
  }
  for (int i = 0; i < u->rx_size; i++) {
    u->rx_buf[i] = 0;
  }
  u->rx_count = 0;
  u->tx_count = 0;
  u->rx_expected = 1;
  u->tx_expected = 1;
  u->running = true;
  return 0;
}

int bsp_uart_write(uart_id_t id, const uint8_t *data, uint32_t size) {
  if (id >= BSP_UARTNUM) {
    return -1;
  }
  uart_t *u = &uart[id];
  if (u->running) {
    u->tx_count = 0;
    u->tx_expected = size;
    XUartLite_Send(&u->instance, data, size);
    return 0;
  }
  return -2;
}

bool bsp_uart_tx_done(uart_id_t id) {
  if (id >= BSP_UARTNUM) {
    return false;
  }
  uart_t *u = &uart[id];
  return u->tx_count == u->tx_expected;
}

int bsp_uart_read(uart_id_t id, uint32_t size) {
  if (id >= BSP_UARTNUM) {
    return -1;
  }
  uart_t *u = &uart[id];
  if (u->running) {
    u->rx_count = 0;
    u->rx_expected = size;
    XUartLite_Recv(&u->instance, u->rx_buf, size);
    return 0;
  }
  return -2;
}

bool bsp_uart_rx_done(uart_id_t id) {
  if (id >= BSP_UARTNUM) {
    return false;
  }
  uart_t *u = &uart[id];
  return u->rx_count == u->rx_expected;
}

int bsp_uart_flush(uart_id_t id) {
  if (id >= BSP_UARTNUM) {
    return -1;
  }
  uart_t *u = &uart[id];
  if (u->running) {
    while (u->tx_count < u->tx_expected || u->rx_count < u->rx_expected) {
    }
    return 0;
  }
  return -2;
}

void bsp_uart_process(void) {
  for (int i = 0; i < BSP_UARTNUM; i++) {
    uart_t *u = &uart[i];
    if (u->running) {
      if (u->rx_count == u->rx_expected) {
        for (int j = 0; j < MAX_UART_CALLBACKS; j++) {
          if (u->callbacks[j] != NULL) {
            u->callbacks[j](u->rx_buf, u->rx_count);
            u->rx_count = 0;
          }
        }
      }
    }
  }
}
