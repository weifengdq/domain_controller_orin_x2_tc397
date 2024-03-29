#include "bsp_timer.h"

#include <stdbool.h>

#include "xil_exception.h"
#include "xinterrupt_wrap.h"
#include "xtmrctr.h"

typedef struct {
  XTmrCtr instance;
  timer_callback_t callbacks[MAX_TIMER_CALLBACKS];
  void *data[MAX_TIMER_CALLBACKS];
  bool running;
  bool isr_flag;
  float ms;
  float period_ms[MAX_TIMER_CALLBACKS];
  int isr_cnt[MAX_TIMER_CALLBACKS];
  int isr_cnt_max[MAX_TIMER_CALLBACKS];
} timer_t;

static timer_t timers[BSP_TIMERNUM];
static uint64_t uptime_ms = 0;

int bsp_timer_register_callback(timer_id_t timer_id, float period_ms,
                                timer_callback_t callback, void *data) {
  if (timer_id >= BSP_TIMERNUM) {
    return -1;
  }
  timer_t *timer = &timers[timer_id];
  for (int i = 0; i < MAX_TIMER_CALLBACKS; i++) {
    if (timer->callbacks[i] == NULL) {
      timer->callbacks[i] = callback;
      timer->data[i] = data;
      timer->period_ms[i] = period_ms;
      return 0;
    }
  }
  return -2;
}

static void timer_isr_handler(void *CallBackRef, u8 TmrCtrNumber) {
  XTmrCtr *InstancePtr = (XTmrCtr *)CallBackRef;
  if (XTmrCtr_IsExpired(InstancePtr, TmrCtrNumber)) {
    timer_t *timer = (timer_t *)InstancePtr;
    if (TmrCtrNumber == BSP_TIMER0) {
      uptime_ms += (uint64_t)timer->ms;
    }
    timer->isr_flag = true;
  }
}

uint64_t bsp_uptime_ms(void) { return uptime_ms; }

int bsp_timer_init(timer_id_t timer_id, uint32_t base_addr, float period_ms,
                   bool auto_start) {
  if (timer_id >= BSP_TIMERNUM) {
    return -1;
  }
  timer_t *timer = &timers[timer_id];
  timer->ms = period_ms;
  // isr_cnt
  for (int i = 0; i < MAX_TIMER_CALLBACKS; i++) {
    timer->isr_cnt[i] = timer->period_ms[i] / period_ms;
    timer->isr_cnt_max[i] = timer->isr_cnt[i];
  }

  int Status = XTmrCtr_Initialize(&timer->instance, base_addr);
  if (Status != XST_SUCCESS) {
    return -2;
  }
  Status = XSetupInterruptSystem(
      &timer->instance, (XInterruptHandler)XTmrCtr_InterruptHandler,
      timer->instance.Config.IntrId, timer->instance.Config.IntrParent,
      XINTERRUPT_DEFAULT_PRIORITY);
  if (Status != XST_SUCCESS) {
    return -3;
  }
  XTmrCtr_SetHandler(&timer->instance, timer_isr_handler, &timer->instance);
  XTmrCtr_SetOptions(
      &timer->instance, timer_id,
      XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);
  XTmrCtr_SetResetValue(&timer->instance, timer_id, (u32)(period_ms * 100000));
  if (auto_start) {
    XTmrCtr_Start(&timer->instance, timer_id);
    timer->running = true;
  }
  return 0;
}

void bsp_timer_start(timer_id_t timer_id) {
  if (timer_id >= BSP_TIMERNUM) {
    return;
  }
  timer_t *timer = &timers[timer_id];
  XTmrCtr_Start(&timer->instance, timer_id);
  timer->running = true;
}

void bsp_timer_stop(timer_id_t timer_id) {
  if (timer_id >= BSP_TIMERNUM) {
    return;
  }
  timer_t *timer = &timers[timer_id];
  XTmrCtr_Stop(&timer->instance, timer_id);
  timer->running = false;
}

void bsp_timer_process(void) {
  for (int i = 0; i < BSP_TIMERNUM; i++) {
    timer_t *timer = &timers[i];
    if (timer->running && timer->isr_flag) {
      for (int j = 0; j < MAX_TIMER_CALLBACKS; j++) {
        if (timer->callbacks[j] != NULL) {
          timer->isr_cnt[j]--;
          if (timer->isr_cnt[j] == 0) {
            timer->isr_cnt[j] = timer->isr_cnt_max[j];
            timer->callbacks[j](timer->data[j]);
          }
        }
      }
      timer->isr_flag = false;
    }
  }
}