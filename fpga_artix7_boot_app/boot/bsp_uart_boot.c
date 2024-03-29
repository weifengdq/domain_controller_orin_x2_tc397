#include "bsp_uart_boot.h"

#include <string.h>

#include "bsp_cpu.h"
#include "bsp_crc.h"
#include "bsp_spi_flash.h"
#include "bsp_timer.h"
#include "bsp_uart.h"

#define UB_DEBUG
#ifdef UB_DEBUG
#include "xil_printf.h"
#define UB_PRINTF xil_printf
#else
#define UB_PRINTF(...)
#endif

#define NEXT_LEN_CMD (16)
#define NEXT_LEN_DATA (16 + 256)

#define IS_BOOT 0xB0000000
#define IS_APP 0xB0000001

typedef enum {
  BOOT_UNKNOWN = 0,
  BOOT_SELF_CHECK,
  BOOT_READY,
  BOOT_SAVE_BRICK,
  BOOT_RESET,
  BOOT_ENTER_BOOT,
  BOOT_ENTER_APP,
  BOOT_NEXT_SET,
  BOOT_ERASE,
  BOOT_WRITE,
  BOOT_READ,
  BOOT_JUMP,
  BOOT_INFO,
  BOOT_STATUS_NUM
} boot_status_t;

void boot_status_unknown(void);
void boot_status_self_check(void);
void boot_status_ready(void);
void boot_status_save_brick(void);
void boot_status_reset(void);
void boot_status_enter_boot(void);
void boot_status_enter_app(void);
void boot_status_next_set(void);
void boot_status_erase(void);
void boot_status_write(void);
void boot_status_read(void);
void boot_status_jump(void);
void boot_status_info(void);
typedef void (*boot_status_func_t)(void);

typedef struct {
  uint32_t crc;
  uint32_t cmd;
  uint32_t addr;
  uint32_t len;
} boot_header_t;

typedef struct {
  boot_header_t header;  // cmd or data header
  boot_status_t status;
  boot_status_func_t status_func[BOOT_STATUS_NUM];
  uart_id_t id;
  uint32_t next_len;
  uint8_t *uart_rx_buf;
  uint8_t *uart_tx_buf;
  uint8_t *flash_rx_buf;
  uint8_t *flash_tx_buf;
  bool is_app;  // true: app, false: boot
  bool is_reading;
  bool is_check_ok;
  uint32_t check_cnt;
  uint32_t app_crc;
  uint32_t app_version;
  uint32_t app_addr;
  uint32_t app_len;
  uint32_t app_crc_cal;
} boot_t;

static boot_t uboot;

static void uart_ack(uint32_t cmd, uint32_t code0, uint32_t code1) {
  *(uint32_t *)(uboot.uart_tx_buf + 4) = 0xFFFFFFFF - cmd;
  *(uint32_t *)(uboot.uart_tx_buf + 8) = code0;
  *(uint32_t *)(uboot.uart_tx_buf + 12) = code1;
  *(uint32_t *)(uboot.uart_tx_buf + 0) =
      bsp_crc32(uboot.uart_tx_buf + 4, 12, 0);
  bsp_uart_write(uboot.id, uboot.uart_tx_buf, 16);
}

static void uart_rx_callback(uint8_t *data, uint32_t size) {
  if (size < 16 || size % 16 != 0) {
    return;
  }
  boot_header_t *header = (boot_header_t *)data;
  if (header->cmd <= BOOT_STATUS_NUM) {
    uint32_t crc0 = bsp_crc32(data + 4, size - 4, 0);
    uint32_t crc1 = *(uint32_t *)data;
    if (crc0 == crc1) {
      memcpy(&uboot.header, header, sizeof(boot_header_t));
      uboot.status = header->cmd;
      if (header->cmd == BOOT_NEXT_SET) {
        uboot.next_len = header->len;
      }
      uart_ack(header->cmd, header->addr, header->len);
    }
  }
  bsp_uart_read(uboot.id,
                uboot.next_len == NEXT_LEN_DATA ? NEXT_LEN_DATA : NEXT_LEN_CMD);
}

void boot_status_func_init(void) {
  uboot.status_func[BOOT_UNKNOWN] = boot_status_unknown;
  uboot.status_func[BOOT_SELF_CHECK] = boot_status_self_check;
  uboot.status_func[BOOT_READY] = boot_status_ready;
  uboot.status_func[BOOT_SAVE_BRICK] = boot_status_save_brick;
  uboot.status_func[BOOT_RESET] = boot_status_reset;
  uboot.status_func[BOOT_ENTER_BOOT] = boot_status_enter_boot;
  uboot.status_func[BOOT_ENTER_APP] = boot_status_enter_app;
  uboot.status_func[BOOT_NEXT_SET] = boot_status_next_set;
  uboot.status_func[BOOT_ERASE] = boot_status_erase;
  uboot.status_func[BOOT_WRITE] = boot_status_write;
  uboot.status_func[BOOT_READ] = boot_status_read;
  uboot.status_func[BOOT_JUMP] = boot_status_jump;
  uboot.status_func[BOOT_INFO] = boot_status_info;
}

int bsp_uart_boot_init(uart_id_t id, uint8_t *uart_rx_buf, uint8_t *uart_tx_buf,
                       uint8_t *flash_rx_buf, uint8_t *flash_tx_buf) {
  memset(&uboot, 0, sizeof(uboot));
  uboot.id = id;
  uboot.uart_rx_buf = uart_rx_buf;
  uboot.uart_tx_buf = uart_tx_buf;
  uboot.flash_rx_buf = flash_rx_buf;
  uboot.flash_tx_buf = flash_tx_buf;
  boot_status_func_init();
  return bsp_uart_register_rx_callback(id, uart_rx_callback);
}

void bsp_uart_boot_process() { uboot.status_func[uboot.status](); }

void boot_status_unknown(void) {
  uint32_t addr = 0;
  uint32_t value = *(uint32_t *)addr;
  if (value == IS_APP) {
    UB_PRINTF("IS_APP\n");
    uboot.is_app = true;
  } else if (value == IS_BOOT) {
    uboot.is_app = false;
  } else {
    uboot.is_app = false;
    UB_PRINTF("ERROR: unknown value: 0x%08X\n", value);
  }
  UB_PRINTF("current: %s\n", uboot.is_app       ? "app"
                             : value == IS_BOOT ? "boot"
                                                : "unknown");
  uboot.status = BOOT_READY;
  uboot.is_app = true;
}

void boot_status_self_check(void) {
  // copy app from flash to ram

  if (uboot.is_reading && (uboot.check_cnt == 1) && !bsp_spi_flash_is_busy()) {
    uint32_t *ptr = (uint32_t *)(uboot.flash_rx_buf + FLASH_RW_EXTRA_BYTES);
    // memcpy(uboot.uart_rx_buf, uboot.flash_rx_buf + FLASH_RW_EXTRA_BYTES, 256);
    // uint32_t *ptr = (uint32_t *)(uboot.uart_rx_buf);
    uboot.app_crc = ptr[0];
    uboot.app_version = ptr[1];
    uboot.app_addr = ptr[2];
    if (uboot.app_addr != APP_FLASH_ADDR) {
      uboot.status = BOOT_SAVE_BRICK;
      UB_PRINTF("ERROR: app_info.addr: 0x%08x != 0x%08x\n", uboot.app_addr,
                APP_FLASH_ADDR);
      return;
    }
    uboot.app_len = ptr[3];
    uboot.app_crc_cal = bsp_crc32((uint8_t *)(&ptr[1]), 12, 0);
    uint32_t app_isr_crc = ptr[4];
    if ((ptr[5] != uboot.app_version) || (ptr[6] != APP_ISR_FLASH_ADDR) ||
        (ptr[7] != APP_ISR_SIZE)) {
      uboot.status = BOOT_SAVE_BRICK;
      UB_PRINTF("ERROR: app_isr_info: 0x%08X, 0x%08X, %d\n", ptr[5], ptr[6],
                ptr[7]);
      return;
    }
    uint32_t app_isr_crc_cal =
        bsp_crc32((uint8_t *)(&ptr[5]), 12 + APP_ISR_SIZE, 0);
    if (app_isr_crc != app_isr_crc_cal) {
      uboot.status = BOOT_SAVE_BRICK;
      UB_PRINTF("ERROR: app_isr_crc: 0x%08X != 0x%08X\n", app_isr_crc,
                app_isr_crc_cal);
      return;
    }
    memcpy((void *)ISR_TEMP_RAM_ADDR, (void *)(&ptr[8]), APP_ISR_SIZE);
    uboot.is_reading = false;
    // UB_PRINTF(
    //     "app_crc: 0x%08X, app_version: 0x%08X, app_addr: 0x%08X, "
    //     "app_len: %d, app_crc_cal: 0x%08X\n",
    //     uboot.app_crc, uboot.app_version, uboot.app_addr, uboot.app_len,
    //     uboot.app_crc_cal);
  } else if (uboot.is_reading && (uboot.check_cnt > 1) &&
             !bsp_spi_flash_is_busy()) {
    // memcpy(uboot.uart_rx_buf, uboot.flash_rx_buf + FLASH_RW_EXTRA_BYTES,
    // 256); uint32_t *ptr = (uint32_t *)(uboot.uart_rx_buf);
    uint32_t *ptr = (uint32_t *)(uboot.flash_rx_buf + FLASH_RW_EXTRA_BYTES);
    int remain_len = uboot.app_len - 256 * (uboot.check_cnt - 1);
    uint32_t current_len = remain_len < 0 ? (256 + remain_len) : 256;
    remain_len = remain_len < 0 ? 0 : remain_len;
    // uboot.app_crc_cal =
    //     bsp_crc32((uint8_t *)ptr, current_len, uboot.app_crc_cal);
    // memcpy((void *)APP_RAM_ADDR, (void *)ptr, current_len);
    uint32_t ram_addr = APP_RAM_ADDR + 256 * (uboot.check_cnt - 2);
    memcpy((void *)ram_addr, (void *)ptr, current_len);
    uboot.app_crc_cal =
        bsp_crc32((uint8_t *)ram_addr, current_len, uboot.app_crc_cal);
    // UB_PRINTF(
    //     "uboot.check_cnt: %d, app_addr: 0x%08X, current_len: %d, remain_len:
    //     "
    //     "%d, uboot.app_crc_cal: 0x%08X\n",
    //     uboot.check_cnt, uboot.app_addr, current_len, remain_len,
    //     uboot.app_crc_cal);
    if (remain_len == 0) {
      if (uboot.app_crc == uboot.app_crc_cal) {
        // uboot.status = BOOT_ENTER_APP;
        uboot.status = BOOT_READY;
        uboot.is_check_ok = true;
        uboot.check_cnt = 0;
        UB_PRINTF("app_crc check ok, enter app\n");
      } else {
        uboot.status = BOOT_SAVE_BRICK;
        uboot.is_check_ok = false;
        uboot.check_cnt = 0;
        UB_PRINTF("ERROR: app_crc: 0x%08X != 0x%08X\n", uboot.app_crc,
                  uboot.app_crc_cal);
      }
      uboot.is_reading = false;
      return;
    }
    uboot.app_addr += current_len;
    uboot.is_reading = false;
  }

  if ((!uboot.is_reading) && (!bsp_spi_flash_is_busy())) {
    uboot.is_reading = true;
    if (uboot.check_cnt == 0) {
      bsp_spi_flash_read(APP_INFO_FLASH_ADDR, APP_ISR_SIZE + 32);
    } else {
      bsp_spi_flash_read(uboot.app_addr, 256);
    }
    uboot.check_cnt += 1;
  }
}

void boot_status_ready(void) {
  uint32_t now = (uint32_t)bsp_uptime_ms();
  if (uboot.is_app && uboot.is_check_ok) {
    UB_PRINTF("[%d.%03d] app crc check ok, jump to app\n", now / 1000,
              now % 1000);
    uboot.status = BOOT_JUMP;
    return;
  }
  if (uboot.is_app && now > 1000) {
    UB_PRINTF(
        "[%d.%03d] save brick timeout, begin app check(copy from flash to ram "
        "and crc "
        "check)\n",
        now / 1000, now % 1000);
    uboot.status = BOOT_SELF_CHECK;
  }
}

void boot_status_save_brick(void) {
  UB_PRINTF("save brick get\n");
  uboot.is_app = false;
  uboot.status = BOOT_READY;
}

void boot_status_reset(void) {
  bsp_cpu_reset();
  while (1)
    ;
}

void boot_status_enter_boot(void) {}

void boot_status_enter_app(void) {}

void boot_status_next_set(void) {
  UB_PRINTF("next_len: %d\n", uboot.next_len);
  uboot.status = BOOT_READY;
}

void boot_status_erase(void) {
  if (bsp_uart_tx_done(uboot.id) && !bsp_spi_flash_is_busy()) {
    UB_PRINTF("erase done: 0x%08x, len: %d\n", uboot.header.addr,
              uboot.header.len);
    uboot.status = BOOT_READY;
  }
  if (!bsp_spi_flash_is_busy()) {
    bsp_spi_flash_erase(uboot.header.addr, uboot.header.len);
  }
}

void boot_status_write(void) {
  if (bsp_uart_tx_done(uboot.id) && !bsp_spi_flash_is_busy()) {
    UB_PRINTF("write done: 0x%08x, len: %d\n", uboot.header.addr,
              uboot.header.len);
    uboot.status = BOOT_READY;
  }
  if (!bsp_spi_flash_is_busy()) {
    bsp_spi_flash_write(uboot.header.addr, uboot.uart_rx_buf + 16,
                        uboot.header.len);
  }
}

void boot_status_read(void) {
  uint32_t len = uboot.header.len <= 256 ? uboot.header.len : 256;
  uint32_t remain = uboot.header.len > len ? uboot.header.len - len : 0;
  if (bsp_uart_tx_done(uboot.id) && !bsp_spi_flash_is_busy()) {
    memcpy(uboot.uart_tx_buf, uboot.flash_rx_buf + FLASH_RW_EXTRA_BYTES, len);
    bsp_uart_write(uboot.id, uboot.uart_tx_buf, len);
    uboot.is_reading = false;
    uboot.header.addr += len;
    uboot.header.len = remain;
    if (remain == 0) {
      uboot.status = BOOT_READY;
      return;
    }
  }
  if ((!uboot.is_reading) && (!bsp_spi_flash_is_busy())) {
    uboot.is_reading = true;
    bsp_spi_flash_read(uboot.header.addr, len);
  }
}

void boot_status_jump(void) {
  // exchange isr_ram and isr_temp_ram
  if (!uboot.is_check_ok) {
    UB_PRINTF("ERROR: You need check first\n");
    uboot.status = BOOT_READY;
    return;
  }
  UB_PRINTF("jump to app\n");
  uint32_t *src = (uint32_t *)ISR_RAM_ADDR;
  uint32_t *dst = (uint32_t *)ISR_TEMP_RAM_ADDR;
  uint32_t size = APP_ISR_SIZE;
  // UB_PRINTF("jump to 0x%08x\n", uboot.header.addr);
  // uint32_t *src = (uint32_t *)0x00000000;
  // uint32_t *dst = (uint32_t *)uboot.header.addr;
  // uint32_t size = uboot.header.len;
  uint32_t temp;
  for (int i = 0; i < size / 4; i++) {
    temp = src[i];
    src[i] = dst[i];
    dst[i] = temp;
  }
  // bsp_timer_stop(BSP_TIMER0);
  // uboot.status = BOOT_ENTER_APP;
  // run from 0
  bsp_cpu_reset();
}

void boot_status_info(void) {
  if (bsp_uart_tx_done(uboot.id)) {
    UB_PRINTF("current: %s\n", uboot.is_app ? "app" : "boot");
    uboot.status = BOOT_READY;
  }
}