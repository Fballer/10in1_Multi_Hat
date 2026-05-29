#pragma once

#include <stddef.h>
#include <stdint.h>

#define FLASH_ROM_MAGIC 0xC0C05DC1u

#define FLASH_TOTAL_SIZE (4u * 1024u * 1024u)
#define FLASH_ROM_SLOT_SIZE (16u * 1024u)
#define FLASH_ROM_RESERVED (256u * 1024u)
#define FLASH_ROM_BASE_OFFSET (FLASH_TOTAL_SIZE - FLASH_ROM_RESERVED)
#define FLASH_ROM_MAX_SLOTS 12

#define SLOT_XBIOS 0
#define SLOT_COCOSDC 1
#define SLOT_FUJINET 2
#define SLOT_RS232 3
#define BANK_SDC_BASE 4
#define BANK_COUNT 8

#define FLASH_STATUS_SDC_MISSING 0x01

#define COPICO_FLASH_CMD_SCAN 1
#define COPICO_FLASH_CMD_CLEAR 2
#define COPICO_FLASH_CMD_REINSTALL 3
#define COPICO_FLASH_CMD_INTERNAL 9

#define COPICO_FLASH_RESULT_IDLE 0
#define COPICO_FLASH_RESULT_BUSY 1
#define COPICO_FLASH_RESULT_OK 2
#define COPICO_FLASH_RESULT_ERROR 3

struct RomSlotHeader {
  uint32_t magic;
  uint16_t size;
  uint16_t map_addr;
  uint16_t checksum;
  uint8_t version;
  uint8_t flags;
  char name[20];
  uint8_t _reserved[224];
};

void copico_flash_rom_init();
uint8_t copico_flash_status_flags();
uint8_t copico_flash_result();

bool copico_flash_queue_command(uint8_t cmd);
bool copico_flash_command_pending();
void copico_flash_service_command();

bool copico_flash_cart_read(uint16_t addr, uint8_t* out_byte);

static inline uint8_t copico_flash_bank_to_slot(uint8_t bank) {
  return static_cast<uint8_t>(BANK_SDC_BASE + bank);
}
