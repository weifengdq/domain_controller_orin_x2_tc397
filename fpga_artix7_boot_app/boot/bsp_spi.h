#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { BSP_SPI0, BSP_SPINUM } spi_id_t;

#define BSP_SPI_TRANSFER_DONE 1052  // XST_SPI_TRANSFER_DONE

#define MAX_SPI_CALLBACKS 2
typedef void (*spi_callback_t)(uint32_t status_event, uint32_t size);

int bsp_spi_init(spi_id_t id, uint32_t base_addr);
int bsp_spi_register_callback(spi_id_t id, spi_callback_t callback);
int bsp_spi_process(void);
int bsp_spi_transfer(spi_id_t id, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t size);

#endif  // BSP_SPI_H
