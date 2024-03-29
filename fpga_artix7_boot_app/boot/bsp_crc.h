#ifndef BSP_CRC_H
#define BSP_CRC_H

#include <stdint.h>

uint32_t bsp_crc32(const uint8_t *data, uint32_t len, uint32_t crc_init);

#endif // BSP_CRC_H

