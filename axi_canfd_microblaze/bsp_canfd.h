#ifndef BSP_CANFD_H
#define BSP_CANFD_H

#include <xcanfd.h>

#include "can.h"

#define BSP_CANFD_DEBUG 1

extern int bsp_canfd_init(XCanFd *InstancePtr, uint32_t BaseAddress,
                          uint32_t BaudRate, float SamplePoint,
                          uint32_t FastBaudRate, float FastSamplePoint);
extern int bsp_canfd_send(XCanFd *InstancePtr, struct canfd_frame *frame);

#endif
