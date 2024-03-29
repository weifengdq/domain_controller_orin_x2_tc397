#include "bsp_gpio.h"
#include "bsp_timer.h"
#include "bsp_uart.h"
#include "xil_printf.h"
#include "xparameters.h"

#define UART_BUF_SIZE (256 + 16)
uint8_t uart0_rx_buf[UART_BUF_SIZE];
uint8_t uart0_tx_buf[UART_BUF_SIZE];

static bool led_state = false;
void timer0_isr(void *data) {
  led_state = !led_state;
  gpio_set(GPIO_LED0, (gpio_value_t)led_state);
}

static uint8_t cmd_info[] = {0xee, 0x8b, 0xdf, 0x7e, 0x0c, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t cmd_info_ack[] = {0x7E, 0xB2, 0xF5, 0xFA, 0xF3, 0xFF,
                                 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};

static uint8_t cmd_reset[] = {0x10, 0xfd, 0xd3, 0x78, 0x04, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint8_t cmd_jump[] = {0x61, 0x62, 0x47, 0x0a, 0x0b, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void uart0_isr(uint8_t *data, uint32_t size) {
  bsp_uart_read(BSP_UART0, 16);
  if (size != 16) {
    return;
  }
  if (memcmp(data, cmd_info, 16) == 0) {
    bsp_uart_write(BSP_UART0, cmd_info_ack, 16);
    xil_printf("current: %s\n",
               (*(uint32_t *)0x00000000) == 0xB0000001 ? "app" : "unknown");
  } else if (memcmp(data, cmd_reset, 16) == 0) {
    (*((void (*)())(0x00)))();
  } else if (memcmp(data, cmd_jump, 16) == 0) {
    uint32_t *src = (uint32_t *)0x00000000;
    uint32_t *dst = (uint32_t *)0x0000FFB0;
    uint32_t size = 0x50;
    uint32_t temp;
    for (int i = 0; i < size / 4; i++) {
      temp = src[i];
      src[i] = dst[i];
      dst[i] = temp;
    }
    // bsp_timer_stop(BSP_TIMER0);
    (*((void (*)())(0x00000000)))();
  } else {
    xil_printf("app: unknown command\n");
  }
}

int main(void) {
  bsp_uart_init(BSP_UART0, XPAR_XUARTLITE_0_BASEADDR, uart0_rx_buf,
                UART_BUF_SIZE, uart0_tx_buf, UART_BUF_SIZE);
  bsp_uart_register_rx_callback(BSP_UART0, uart0_isr);
  bsp_uart_read(BSP_UART0, 16);

  gpio_init(GPIO_GROUP0, XPAR_AXI_GPIO_0_BASEADDR);
  gpio_dir(GPIO_LED0, GPIO_OUT);
  gpio_set(GPIO_LED0, GPIO_HIGH);

  bsp_timer_register_callback(BSP_TIMER0, 2000, timer0_isr, NULL);
  bsp_timer_init(BSP_TIMER0, XPAR_AXI_TIMER_0_BASEADDR, 1, true);

  // xil_printf("current: %s\n",
  //            (*(uint32_t *)0x00000000) == 0xB0000001 ? "app" : "unknown");
  if ((*(uint32_t *)0x00000000) == 0xB0000001) {
    xil_printf("current: app\n");
  } else {
    xil_printf("current: unknown, 0x%08X\n", (*(uint32_t *)0x00000000));
  }

  while (1) {
    bsp_uart_process();
    bsp_timer_process();
  }
}