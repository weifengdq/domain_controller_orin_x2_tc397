#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

typedef enum {
  GPIO_GROUP0 = 0,
  GPIO_GROUP_NUM
} gpio_group_t;

typedef enum {
  GPIO_LED0 = 0,
  GPIO_NUM
} gpio_pin_t;

typedef enum {
  GPIO_IN  = 0,
  GPIO_OUT = 1
} gpio_dir_t;

typedef enum {
  GPIO_HIGH = 1,
  GPIO_LOW  = 0
} gpio_value_t;

int gpio_init(gpio_group_t group, uint32_t base_addr);
void gpio_dir(gpio_pin_t pin, gpio_dir_t dir);
void gpio_set(gpio_pin_t pin, gpio_value_t value);

#endif // BSP_GPIO_H
