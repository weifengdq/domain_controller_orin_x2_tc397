#ifndef CAN_H
#define CAN_H

#include <stdint.h>

/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */

/* CAN payload length and DLC definitions according to ISO 11898-1 */
#define CAN_MAX_DLC 8
#define CAN_MAX_RAW_DLC 15
#define CAN_MAX_DLEN 8

/* CAN FD payload length and DLC definitions according to ISO 11898-7 */
#define CANFD_MAX_DLC 15
#define CANFD_MAX_DLEN 64

/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28     : CAN identifier (11/29 bit)
 * bit 29       : error message frame flag (0 = data frame, 1 = error message)
 * bit 30       : remote transmission request flag (1 = rtr frame)
 * bit 31       : frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */
typedef uint32_t canid_t;

#define CAN_SFF_ID_BITS 11
#define CAN_EFF_ID_BITS 29

#define CANFD_BRS 0x01 /* bit rate switch (second bitrate for payload data) */
#define CANFD_ESI 0x02 /* error state indicator of the transmitting node */
#define CANFD_FDF 0x04 /* mark CAN FD for dual use of struct canfd_frame */

struct can_frame {
  canid_t can_id; /* 32 bit CAN_ID + EFF/RTR/ERR flags */
  union {
    /* CAN frame payload length in byte (0 .. CAN_MAX_DLEN)
     * was previously named can_dlc so we need to carry that
     * name for legacy support
     */
    uint8_t len;
    uint8_t can_dlc;         /* deprecated */
  } __attribute__((packed)); /* disable padding added in some ABIs */
  uint8_t __pad;             /* padding */
  uint8_t __res0;            /* reserved / padding */
  uint8_t len8_dlc; /* optional DLC for 8 byte payload length (9 .. 15) */
  uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};

struct canfd_frame {
  canid_t can_id; /* 32 bit CAN_ID + EFF/RTR/ERR flags */
  uint8_t len;    /* frame payload length in byte */
  uint8_t flags;  /* additional flags for CAN FD */
  uint8_t __res0; /* reserved / padding */
  uint8_t __res1; /* reserved / padding */
  uint8_t data[CANFD_MAX_DLEN] __attribute__((aligned(8)));
};

#define CAN_MTU (sizeof(struct can_frame))
#define CANFD_MTU (sizeof(struct canfd_frame))

#endif