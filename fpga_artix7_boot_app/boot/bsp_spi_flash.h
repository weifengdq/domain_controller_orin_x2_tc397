#ifndef BSP_SPI_FLASH_H
#define BSP_SPI_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_spi.h"

// C:\Xilinx\Vitis\2023.2\data\embeddedsw\XilinxProcessorIPLib\drivers\spi_v4_11\examples\xspi_numonyx_flash_quad_example.c

#define FLASH_RW_EXTRA_BYTES 4 /* Read/Write extra bytes */
#define PAGE_SIZE 256
#define SECTOR_SIZE 65536

int bsp_spi_flash_init(spi_id_t id, uint8_t *rx_buf, uint8_t *tx_buf);
int bsp_spi_flash_erase(uint32_t addr, uint32_t len);
int bsp_spi_flash_write(uint32_t addr, uint8_t *data, uint32_t len);
int bsp_spi_flash_read(uint32_t addr, uint32_t len);
int bsp_spi_flash_process(void);

bool bsp_spi_flash_is_busy(void);
uint32_t bsp_spi_flash_get_status(void);
int bsp_spi_flash_error_count(void);
int bsp_spi_flash_reset_error_count(void);
int bsp_spi_flash_status_reset(void);

#endif  // BSP_SPI_FLASH_H
