#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { BSP_TIMER0 = 0, BSP_TIMERNUM } timer_id_t;

#define MAX_TIMER_CALLBACKS 4
typedef void (*timer_callback_t)(void *data);
int bsp_timer_register_callback(timer_id_t timer_id, float period_ms,
                                timer_callback_t callback, void *data);
int bsp_timer_init(timer_id_t timer_id, uint32_t base_addr, float period_ms,
                   bool auto_start);
void bsp_timer_start(timer_id_t timer_id);
void bsp_timer_stop(timer_id_t timer_id);
void bsp_timer_process(void);

uint64_t bsp_uptime_ms(void);

#endif