#include "bsp_gpio.h"

#include "xgpio.h"

static XGpio gpio[GPIO_GROUP_NUM];

int gpio_init(gpio_group_t group, uint32_t base_addr) {
  if (group >= GPIO_GROUP_NUM) {
    return -1;
  }
  int Status = XGpio_Initialize(&gpio[group], base_addr);
  if (Status != XST_SUCCESS) {
    return -2;
  }
  return 0;
}

void gpio_dir(gpio_pin_t pin, gpio_dir_t dir) {
  XGpio_SetDataDirection(&gpio[GPIO_GROUP0], pin + 1, ~(dir << pin));
}

void gpio_set(gpio_pin_t pin, gpio_value_t value) {
  XGpio_DiscreteWrite(&gpio[GPIO_GROUP0], pin + 1, value << pin);
}