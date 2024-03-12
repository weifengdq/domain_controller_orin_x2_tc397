#include "bsp_canfd.h"

#include <stdbool.h>
#include <xcanfd.h>
#include <xinterrupt_wrap.h>
#include <xparameters.h>

#if BSP_CANFD_DEBUG
#include <xil_printf.h>
#define bsp_canfd_debug_printf(...) xil_printf(__VA_ARGS__)
#else
#define bsp_canfd_debug_printf(...)
#endif

static void SendHandler(void *CallBackRef) {}

static void RecvHandler(void *CallBackRef) {
  XCanFd *CanPtr = (XCanFd *)CallBackRef;
  int Status;
  u32 RxFrame[CANFD_MTU];

  /* Check for the design 1 - MailBox 0 - Sequential */
  if (XCANFD_GET_RX_MODE(CanPtr) == 1) {
    Status = XCanFd_Recv_Mailbox(CanPtr, RxFrame);
  } else {
    Status = XCanFd_Recv_Sequential(CanPtr, RxFrame);
  }

  u32 id1 = (RxFrame[0] >> (u32)XCANFD_IDR_ID1_SHIFT) & (u32)0x7FF;
  u32 is_extended = (RxFrame[0] >> (u32)XCANFD_IDR_IDE_SHIFT) & (u32)0x1;
  u32 id2 = (RxFrame[0] >> (u32)XCANFD_IDR_ID2_SHIFT) & (u32)0x3FFFF;
  u32 is_remote = is_extended
                      ? (RxFrame[0] & 0x01)
                      : ((RxFrame[0] >> (u32)XCANFD_IDR_SRR_SHIFT) & (u32)0x1);

  /* Get the Dlc inthe form of bytes */
  u32 len = XCanFd_GetDlc2len(RxFrame[1] & XCANFD_DLCR_DLC_MASK, EDL_CANFD);
  if (Status != XST_SUCCESS) {
    bsp_canfd_debug_printf("Error: XCanFd_Recv returned %d\n", Status);
    return;
  }
  u32 is_brs = RxFrame[1] & XCANFD_DLCR_BRS_MASK ? 1 : 0;
  u32 is_fdf = RxFrame[1] & XCANFD_DLCR_EDL_MASK ? 1 : 0;
  // bsp_canfd_debug_printf("%08X ", RxFrame[0]);
  u8 *FramePtr = (u8 *)(&RxFrame[2]);
  if (is_extended) {
    bsp_canfd_debug_printf("%08X ", id1 << 18 | id2);
  } else {
    bsp_canfd_debug_printf("%03X ", id1);
  }
  if (is_remote) {
    bsp_canfd_debug_printf("R [%d]", len);
  } else {
    bsp_canfd_debug_printf("D ");
    if (is_fdf) {
      bsp_canfd_debug_printf("F ");
    } else {
      bsp_canfd_debug_printf("- ");
    }
    if (is_brs) {
      bsp_canfd_debug_printf("B ");
    } else {
      bsp_canfd_debug_printf("- ");
    }
    if ((!is_fdf) && (!is_brs)) {
      bsp_canfd_debug_printf("[%d] ", len);
    } else {
      bsp_canfd_debug_printf("[%02d] ", len);
    }
    for (int i = 0; i < len; i++) {
      bsp_canfd_debug_printf("%02X ", FramePtr[i]);
    }
  }
  bsp_canfd_debug_printf("\n");
}

static void ErrorHandler(void *CallBackRef, u32 ErrorMask) {}

static void EventHandler(void *CallBackRef, u32 IntrMask) {
  XCanFd *CanPtr = (XCanFd *)CallBackRef;
  if (IntrMask & XCANFD_IXR_BSOFF_MASK) {
    /*
     * The CAN device requires 128 * 11 consecutive recessive bits
     * to recover from bus off.
     */
    XCanFd_Pee_BusOff_Handler(CanPtr);
    return;
  }

  if (IntrMask & XCANFD_IXR_PEE_MASK) {
    XCanFd_Pee_BusOff_Handler(CanPtr);
    return;
  }
}

int bsp_canfd_init(XCanFd *InstancePtr, uint32_t BaseAddress, uint32_t BaudRate,
                   float SamplePoint, uint32_t FastBaudRate,
                   float FastSamplePoint) {
  XCanFd_Config *ConfigPtr = XCanFd_LookupConfig(BaseAddress);
  if (ConfigPtr == NULL) {
    bsp_canfd_debug_printf("Error: XCanFd_LookupConfig returned NULL\n");
    return -1;
  } else {
    bsp_canfd_debug_printf("XCanFd_Config:\n");
    bsp_canfd_debug_printf("  BaseAddress: 0x%08X\n", ConfigPtr->BaseAddress);
    bsp_canfd_debug_printf("  Rx_Mode: %s\n",
                           ConfigPtr->Rx_Mode ? "Mailbox" : "Sequential");
    bsp_canfd_debug_printf("  NumofRxMbBuf: %d\n", ConfigPtr->NumofRxMbBuf);
    bsp_canfd_debug_printf("  NumofTxBuf: %d\n", ConfigPtr->NumofTxBuf);
    bsp_canfd_debug_printf("  IntrId: %d\n", ConfigPtr->IntrId);
    bsp_canfd_debug_printf("  IntrParent: 0x%08X\n", ConfigPtr->IntrParent);
  }
  int Status =
      XCanFd_CfgInitialize(InstancePtr, ConfigPtr, ConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    bsp_canfd_debug_printf("Error: XCanFd_CfgInitialize returned %d\n", Status);
    return -2;
  } else {
    bsp_canfd_debug_printf("XCanFd_CfgInitialize: Success\n");
  }

  // config mode
  XCanFd_EnterMode(InstancePtr, XCANFD_MODE_CONFIG);
  while (XCanFd_GetMode(InstancePtr) != XCANFD_MODE_CONFIG)
    ;
  bsp_canfd_debug_printf("XCanFd_EnterMode: XCANFD_MODE_CONFIG\n");

  // 80MHz / (3 + 1) = 20MHz, 20MHz / (1 + (30 + 1) + (7 + 1)) = 500KHz
  // XCanFd_SetBitTiming(InstancePtr, 8, 7, 30);
  XCanFd_SetBaudRatePrescaler(InstancePtr, 0x3);
  u16 total_tq = 20000000 / BaudRate;
  u16 tseg1 = (u16)(SamplePoint * total_tq) - 2;
  u8 tseg2 = (u8)(total_tq - tseg1 - 3);
  u8 sjw = tseg2 + 1;
  XCanFd_SetBitTiming(InstancePtr, sjw, tseg2, tseg1);

  // 80MHz, 80 / (1 + (30 + 1) + (7 + 1)) = 2MHz
  // XCanFd_SetFBitTiming(InstancePtr, 8, 7, 30);
  XCanFd_SetFBaudRatePrescaler(InstancePtr, 0x0);
  u16 ftotal_tq = 80000000 / FastBaudRate;
  u8 ftseg1 = (u8)(FastSamplePoint * ftotal_tq) - 2;
  u8 ftseg2 = (u8)(ftotal_tq - ftseg1 - 3);
  u8 fsjw = ftseg2 + 1;
  XCanFd_SetFBitTiming(InstancePtr, fsjw, ftseg2, ftseg1);

  // TDC, 0~31
  // XCanFd_Set_Tranceiver_Delay_Compensation(InstancePtr, 0x3);

  XCanFd_SetBitRateSwitch_DisableNominal(InstancePtr);
  bsp_canfd_debug_printf("XCanFd: %d@0.%d, %d@0.%d\n", BaudRate,
                         (int)(SamplePoint * 1000), FastBaudRate,
                         (int)(FastSamplePoint * 1000));

  if (XCANFD_GET_RX_MODE(InstancePtr) == 0) {
    bsp_canfd_debug_printf(
        "RX_MODE Sequential Filter: XCANFD_AFR_UAF_ALL_MASK\n");
    XCanFd_AcceptFilterDisable(InstancePtr, XCANFD_AFR_UAF_ALL_MASK);
    // XCanFd_AcceptFilterSet(InstancePtr, 0, 0xFFFFFFFF, 0);
    XCanFd_AcceptFilterEnable(InstancePtr, XCANFD_AFR_UAF_ALL_MASK);
  } else {
    bsp_canfd_debug_printf("RX_MODE Mailbox Filter: Need to be implemented\n");
  }

  XCanFd_SetHandler(InstancePtr, XCANFD_HANDLER_SEND, (void *)SendHandler,
                    (void *)InstancePtr);
  XCanFd_SetHandler(InstancePtr, XCANFD_HANDLER_RECV, (void *)RecvHandler,
                    (void *)InstancePtr);
  XCanFd_SetHandler(InstancePtr, XCANFD_HANDLER_ERROR, (void *)ErrorHandler,
                    (void *)InstancePtr);
  XCanFd_SetHandler(InstancePtr, XCANFD_HANDLER_EVENT, (void *)EventHandler,
                    (void *)InstancePtr);
  Status =
      XSetupInterruptSystem(InstancePtr, &XCanFd_IntrHandler, ConfigPtr->IntrId,
                            ConfigPtr->IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (Status != XST_SUCCESS) {
    bsp_canfd_debug_printf("Error: XSetupInterruptSystem returned %d\n",
                           Status);
    return 1;
  } else {
    bsp_canfd_debug_printf("XSetupInterruptSystem: Success\n");
  }
  XCanFd_InterruptEnable(InstancePtr, XCANFD_IXR_ALL);
  XCanFd_EnterMode(InstancePtr, XCANFD_MODE_NORMAL);
  while (XCanFd_GetMode(InstancePtr) != XCANFD_MODE_NORMAL)
    ;
  bsp_canfd_debug_printf("XCanFd_EnterMode: XCANFD_MODE_NORMAL\n");

  return 0;
}

int bsp_canfd_send(XCanFd *InstancePtr, struct canfd_frame *frame) {
  bool is_extended = frame->can_id & CAN_EFF_FLAG ? true : false;
  bool is_remote = frame->can_id & CAN_RTR_FLAG ? true : false;
  bool is_fd = frame->flags & CANFD_FDF ? true : false;
  bool is_brs = frame->flags & CANFD_BRS ? true : false;
  bool is_esi = frame->flags & CANFD_ESI ? true : false;
  u32 TxFrame[CANFD_MTU];
  TxFrame[0] = XCanFd_CreateIdValue(
      CAN_SFF_MASK & (is_extended ? ((frame->can_id & CAN_EFF_MASK) >> 18)
                                  : frame->can_id),
      is_extended ? 1 : (u32)is_remote, (u32)is_extended,
      (u32)is_extended ? (frame->can_id & 0x3FFFF) : 0,
      is_extended ? (u32)is_remote : 0);
  if ((!is_fd) && (!is_brs)) {
    TxFrame[1] = XCanFd_CreateDlcValue(frame->len);
  } else {
    if (is_brs) {
      TxFrame[1] =
          XCanFd_Create_CanFD_Dlc_BrsValue(XCanFd_GetLen2Dlc(frame->len));
    } else {
      TxFrame[1] = XCanFd_Create_CanFD_DlcValue(XCanFd_GetLen2Dlc(frame->len));
    }
  }
  u8 *FramePtr = (u8 *)(&TxFrame[2]);
  for (int i = 0; i < frame->len; i++) {
    FramePtr[i] = frame->data[i];
  }
  u32 TxBufferNumber;
  int status = XCanFd_Send(InstancePtr, TxFrame, &TxBufferNumber);
  if (status == XST_FIFO_NO_ROOM) {
    bsp_canfd_debug_printf("Error: XCanFd_Send returned XST_FIFO_NO_ROOM\n");
    return -1;
  }
  if (status != XST_SUCCESS) {
    bsp_canfd_debug_printf("Error: XCanFd_Send returned %d\n", status);
    return -2;
  }
  return 0;
}