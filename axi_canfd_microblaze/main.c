#include <xil_printf.h>
#include <xparameters.h>

#include "bsp_canfd.h"

int main() {
  xil_printf("============================================\n");

  XCanFd CanFd0;
  int Status =
      bsp_canfd_init(&CanFd0, XPAR_CANFD_0_BASEADDR, 500000, 0.8, 4000000, 0.8);
  if (Status != 0) {
    xil_printf("Error: bsp_canfd_init returned %d\n", Status);
    return -1;
  }

  for (int i = 0; i < 4; i++) {
    struct canfd_frame frame = {
        .can_id = 0x123,
        .len = 8,
        .flags = 0,
        .__res0 = 0,
        .__res1 = 0,
        .data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
                 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
                 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
                 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
                 0x3C, 0x3D, 0x3E, 0x3F},
    };
    // std can
    frame.can_id = 0x123;
    frame.len = 8;
    frame.flags = 0;
    int Status = bsp_canfd_send(&CanFd0, &frame);
    // std can remote
    frame.can_id = 0x124 | CAN_RTR_FLAG;
    frame.len = 8;
    frame.flags = 0;
    Status |= bsp_canfd_send(&CanFd0, &frame);
    // std can fd
    frame.can_id = 0x125;
    frame.len = 64;
    frame.flags = CANFD_FDF;
    Status |= bsp_canfd_send(&CanFd0, &frame);
    // std can fd brs
    frame.can_id = 0x126;
    frame.len = 64;
    frame.flags = CANFD_FDF | CANFD_BRS;
    Status |= bsp_canfd_send(&CanFd0, &frame);

    // ext can
    frame.can_id = 0x12345678 | CAN_EFF_FLAG;
    frame.len = 8;
    frame.flags = 0;
    Status |= bsp_canfd_send(&CanFd0, &frame);
    // ext can remote
    frame.can_id = 0x12345679 | CAN_EFF_FLAG | CAN_RTR_FLAG;
    frame.len = 8;
    frame.flags = 0;
    Status |= bsp_canfd_send(&CanFd0, &frame);
    // ext can fd
    frame.can_id = 0x1234567A | CAN_EFF_FLAG;
    frame.len = 64;
    frame.flags = CANFD_FDF;
    Status |= bsp_canfd_send(&CanFd0, &frame);
    // ext can fd brs
    frame.can_id = 0x1234567B | CAN_EFF_FLAG;
    frame.len = 64;
    frame.flags = CANFD_FDF | CANFD_BRS;
    Status |= bsp_canfd_send(&CanFd0, &frame);

    if (Status != 0) {
      xil_printf("Error: bsp_canfd_send %d returned %d\n", i, Status);
      return -1;
    }
  }

  while (1) {
  }

  return 0;
}
