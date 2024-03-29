#ifndef BSP_UART_BOOT_H
#define BSP_UART_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_uart.h"

#define SAVE_BRICK_MS 5
#define APP_INFO_FLASH_ADDR 0x003F0000
#define APP_INFO_SIZE 0x10
#define APP_ISR_INFO_FLASH_ADDR 0x003F0010
#define APP_ISR_INFO_SIZE 0x10
#define APP_ISR_FLASH_ADDR 0x003F0020
#define APP_ISR_SIZE 0x50
#define APP_FLASH_ADDR 0x00400000
#define ISR_RAM_ADDR 0x00000000
#define ISR_TEMP_RAM_ADDR 0x0000FFB0
#define APP_RAM_ADDR 0x00010000

int bsp_uart_boot_init(uart_id_t id, uint8_t *uart_rx_buf, uint8_t *uart_tx_buf,
                       uint8_t *flash_rx_buf, uint8_t *flash_tx_buf);

void bsp_uart_boot_process(void);

#endif  // BSP_UART_BOOT_H
