#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { BSP_UART0 = 0, BSP_UARTNUM } uart_id_t;
#define MAX_UART_CALLBACKS 2
typedef void (*uart_callback_t)(uint8_t *data, uint32_t size);
int bsp_uart_init(uart_id_t id, uint32_t base_addr, uint8_t *rx_buf,
                  uint32_t rx_size, uint8_t *tx_buf, uint32_t tx_size);
int bsp_uart_write(uart_id_t id, const uint8_t *data, uint32_t size);
int bsp_uart_read(uart_id_t id, uint32_t size);
bool bsp_uart_tx_done(uart_id_t id);
bool bsp_uart_rx_done(uart_id_t id);
int bsp_uart_register_rx_callback(uart_id_t id, uart_callback_t callback);
void bsp_uart_process(void);

#endif  // BSP_UART_H
