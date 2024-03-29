#include "bsp_spi.h"
#include "bsp_spi_flash.h"
#include "bsp_uart.h"
#include "bsp_uart_boot.h"
#include "xil_printf.h"
#include "bsp_timer.h"
#include "xparameters.h"

#define UART_BUF_SIZE (256 + 16)
uint8_t uart0_rx_buf[UART_BUF_SIZE];
uint8_t uart0_tx_buf[UART_BUF_SIZE];
uint8_t flash0_rx_buf[256 + 4 + 4];
uint8_t flash0_tx_buf[256 + 4];

int main(void) {
  bsp_timer_init(BSP_TIMER0, XPAR_AXI_TIMER_0_BASEADDR, 1, true);

  bsp_uart_init(BSP_UART0, XPAR_XUARTLITE_0_BASEADDR, uart0_rx_buf,
                UART_BUF_SIZE, uart0_tx_buf, UART_BUF_SIZE);
  bsp_uart_read(BSP_UART0, 16);

  bsp_spi_init(BSP_SPI0, XPAR_XSPI_0_BASEADDR);
  bsp_spi_flash_init(BSP_SPI0, flash0_rx_buf, flash0_tx_buf);

  bsp_uart_boot_init(BSP_UART0, uart0_rx_buf, uart0_tx_buf, flash0_rx_buf,
                     flash0_tx_buf);

  while (1) {
    bsp_uart_process();
    bsp_spi_process();
    bsp_spi_flash_process();
    bsp_uart_boot_process();
  }
  return 0;
}