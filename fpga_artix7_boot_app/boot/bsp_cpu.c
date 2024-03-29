#include "bsp_cpu.h"

#include "xil_exception.h"

void bsp_cpu_reset(void) {
  // microblaze_disable_interrupts();
  (*((void (*)())(0x00)))();  // restart
}