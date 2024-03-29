#include "bsp_spi_flash.h"

#include "bsp_spi.h"

// C:\Xilinx\Vitis\2023.2\data\embeddedsw\XilinxProcessorIPLib\drivers\spi_v4_11\examples\xspi_numonyx_flash_quad_example.c

// NUMONYX N25Q128
#define COMMAND_PAGE_PROGRAM 0x02   /* Page Program command */
#define COMMAND_QUAD_WRITE 0x32     /* Quad Input Fast Program */
#define COMMAND_RANDOM_READ 0x03    /* Random read command */
#define COMMAND_DUAL_READ 0x3B      /* Dual Output Fast Read */
#define COMMAND_DUAL_IO_READ 0xBB   /* Dual IO Fast Read */
#define COMMAND_QUAD_READ 0x6B      /* Quad Output Fast Read */
#define COMMAND_QUAD_IO_READ 0xEB   /* Quad IO Fast Read */
#define COMMAND_WRITE_ENABLE 0x06   /* Write Enable command */
#define COMMAND_SECTOR_ERASE 0xD8   /* Sector Erase command */
#define COMMAND_BULK_ERASE 0xC7     /* Bulk Erase command */
#define COMMAND_STATUSREG_READ 0x05 /* Status read command */

#define WRITE_ENABLE_BYTES 1 /* Write Enable bytes */
#define SECTOR_ERASE_BYTES 4 /* Sector erase extra bytes */
#define BULK_ERASE_BYTES 1   /* Bulk erase extra bytes */
#define STATUS_READ_BYTES 2  /* Status read bytes count */
#define STATUS_WRITE_BYTES 2 /* Status write bytes count */

#define FLASH_SR_IS_READY_MASK 0x01 /* Ready mask */

#define DUAL_READ_DUMMY_BYTES 2
#define QUAD_READ_DUMMY_BYTES 4

#define DUAL_IO_READ_DUMMY_BYTES 2
#define QUAD_IO_READ_DUMMY_BYTES 5

volatile static bool TransferInProgress;
static int ErrorCount;

enum {
  SPI_FLASH_UNINIT = 0,
  SPI_FLASH_READY = 1,
  SPI_FLASH_WRITE_ENABLING = 2,
  SPI_FLASH_ERASING = 4,
  SPI_FLASH_WRITING = 8,
  SPI_FLASH_READING = 16,
  SPI_FLASH_ERROR = 32
};

typedef union {
  struct {
    uint32_t ready : 1;
    uint32_t write_enabling : 1;
    uint32_t erasing : 1;
    uint32_t writing : 1;
    uint32_t reading : 1;
    uint32_t busy : 1;
    uint32_t error : 1;
  };
  uint32_t value;
} spi_flash_status_t;

typedef struct {
  spi_id_t id;
  spi_flash_status_t status;
  uint32_t addr;
  uint32_t byte_count;
  uint8_t *rx_buf;
  uint8_t *tx_buf;
} spi_flash_t;

static spi_flash_t spi_flash;

bool bsp_spi_flash_is_busy(void) {
  return spi_flash.status.value != SPI_FLASH_READY;
}

uint32_t bsp_spi_flash_get_status(void) { return spi_flash.status.value; }

static void spi_handler(uint32_t StatusEvent, uint32_t ByteCount) {
  TransferInProgress = false;
  if (StatusEvent != BSP_SPI_TRANSFER_DONE) {
    ErrorCount++;
  }
  spi_flash.status.ready = 1;
}

int bsp_spi_flash_error_count(void) { return ErrorCount; }

int bsp_spi_flash_reset_error_count(void) {
  ErrorCount = 0;
  return 0;
}

int bsp_flash_read_id(spi_id_t id, uint8_t *rx_buf, uint8_t *tx_buf) {
  tx_buf[0] = 0x9F;
  tx_buf[1] = 0xFF;
  tx_buf[2] = 0xFF;
  tx_buf[3] = 0xFF;
  tx_buf[4] = 0xFF;
  TransferInProgress = true;
  return bsp_spi_transfer(id, tx_buf, rx_buf, 5);
}


int bsp_spi_flash_init(spi_id_t id, uint8_t *rx_buf, uint8_t *tx_buf) {
  spi_flash.status.value = SPI_FLASH_UNINIT;
  bsp_spi_register_callback(id, spi_handler);
  spi_flash.id = id;
  spi_flash.status.value = SPI_FLASH_READY;
  spi_flash.rx_buf = rx_buf;
  spi_flash.tx_buf = tx_buf;
  bsp_flash_read_id(id, rx_buf, tx_buf);
  return 0;
}

int bsp_spi_flash_status_reset(void) {
  spi_flash.status.value = SPI_FLASH_READY;
  return 0;
}

int bsp_spi_flash_erase(uint32_t addr, uint32_t len) {
  if (spi_flash.status.value != SPI_FLASH_READY) {
    return -1;
  }
  spi_flash.status.value = SPI_FLASH_WRITE_ENABLING | SPI_FLASH_ERASING;
  spi_flash.addr = addr;
  spi_flash.byte_count = len;
  return 0;
}

int bsp_spi_flash_write(uint32_t addr, uint8_t *data, uint32_t len) {
  if (spi_flash.status.value != SPI_FLASH_READY) {
    return -1;
  }
  spi_flash.status.value = SPI_FLASH_WRITE_ENABLING | SPI_FLASH_WRITING;
  spi_flash.addr = addr;
  spi_flash.byte_count = len;
  // if data addr is not spi_flash.tx_buf addr + FLASH_RW_EXTRA_BYTES, copy data
  if (data != spi_flash.tx_buf + FLASH_RW_EXTRA_BYTES) {
    for (int i = 0; i < PAGE_SIZE; i++) {
      spi_flash.tx_buf[i + FLASH_RW_EXTRA_BYTES] = data[i];
    }
  }
  return 0;
}

int bsp_spi_flash_read(uint32_t addr, uint32_t len) {
  if (spi_flash.status.value != SPI_FLASH_READY) {
    return -1;
  }
  spi_flash.status.value = SPI_FLASH_READING;
  spi_flash.addr = addr;
  spi_flash.byte_count = len;
  return 0;
}

int bsp_spi_flash_process(void) {
  if (spi_flash.status.write_enabling) {
    spi_flash.tx_buf[0] = COMMAND_WRITE_ENABLE;
    TransferInProgress = true;
    int Status = bsp_spi_transfer(spi_flash.id, spi_flash.tx_buf,
                                  spi_flash.rx_buf, WRITE_ENABLE_BYTES);
    spi_flash.status.write_enabling = 0;
    spi_flash.status.ready = 0;
    if (Status != 0) {
      return -1;
    }
    return 0;
  }

  if (spi_flash.status.ready && spi_flash.status.erasing) {
    int sector_number = spi_flash.byte_count % SECTOR_SIZE == 0
                            ? spi_flash.byte_count / SECTOR_SIZE
                            : spi_flash.byte_count / SECTOR_SIZE + 1;
    if (sector_number > 0) {
      spi_flash.tx_buf[0] = COMMAND_SECTOR_ERASE;
      spi_flash.tx_buf[1] = (spi_flash.addr >> 16) & 0xFF;
      spi_flash.tx_buf[2] = (spi_flash.addr >> 8) & 0xFF;
      spi_flash.tx_buf[3] = spi_flash.addr & 0xFF;
      TransferInProgress = true;
      int Status = bsp_spi_transfer(spi_flash.id, spi_flash.tx_buf,
                                    spi_flash.rx_buf, SECTOR_ERASE_BYTES);
      spi_flash.addr += SECTOR_SIZE;
      spi_flash.byte_count = spi_flash.byte_count > SECTOR_SIZE
                                 ? spi_flash.byte_count - SECTOR_SIZE
                                 : 0;
      spi_flash.status.erasing = spi_flash.byte_count > 0;
      spi_flash.status.ready = 0;
      if (Status != 0) {
        return -1;
      }
      return 0;
    }
  }

  if (spi_flash.status.ready && spi_flash.status.writing) {
    int page_number = spi_flash.byte_count % PAGE_SIZE == 0
                          ? spi_flash.byte_count / PAGE_SIZE
                          : spi_flash.byte_count / PAGE_SIZE + 1;
    if (page_number > 0) {
      spi_flash.tx_buf[0] = COMMAND_PAGE_PROGRAM;
      spi_flash.tx_buf[1] = (spi_flash.addr >> 16) & 0xFF;
      spi_flash.tx_buf[2] = (spi_flash.addr >> 8) & 0xFF;
      spi_flash.tx_buf[3] = spi_flash.addr & 0xFF;
      TransferInProgress = true;
      int Status =
          bsp_spi_transfer(spi_flash.id, spi_flash.tx_buf, spi_flash.rx_buf,
                           PAGE_SIZE + FLASH_RW_EXTRA_BYTES);
      spi_flash.addr += PAGE_SIZE;
      spi_flash.byte_count = spi_flash.byte_count > PAGE_SIZE
                                 ? spi_flash.byte_count - PAGE_SIZE
                                 : 0;
      spi_flash.status.writing = spi_flash.byte_count > 0;
      spi_flash.status.ready = 0;
      if (Status != 0) {
        return -1;
      }
      return 0;
    }
  }

  if (spi_flash.status.reading) {
    int page_number = spi_flash.byte_count % PAGE_SIZE == 0
                          ? spi_flash.byte_count / PAGE_SIZE
                          : spi_flash.byte_count / PAGE_SIZE + 1;
    if (page_number > 0) {
      spi_flash.tx_buf[0] = COMMAND_RANDOM_READ;
      spi_flash.tx_buf[1] = (spi_flash.addr >> 16) & 0xFF;
      spi_flash.tx_buf[2] = (spi_flash.addr >> 8) & 0xFF;
      spi_flash.tx_buf[3] = spi_flash.addr & 0xFF;
      TransferInProgress = true;
      int Status =
          bsp_spi_transfer(spi_flash.id, spi_flash.tx_buf, spi_flash.rx_buf,
                           PAGE_SIZE + FLASH_RW_EXTRA_BYTES);
      spi_flash.addr += PAGE_SIZE;
      spi_flash.byte_count = spi_flash.byte_count > PAGE_SIZE
                                 ? spi_flash.byte_count - PAGE_SIZE
                                 : 0;
      spi_flash.status.reading = spi_flash.byte_count > 0;
      spi_flash.status.ready = 0;
      if (Status != 0) {
        return -1;
      }
      return 0;
    }
  }

  return 0;
}