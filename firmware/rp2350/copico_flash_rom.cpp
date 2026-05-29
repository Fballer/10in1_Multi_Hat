#include "copico_flash_rom.h"

#include "copico_esp32_bridge.h"
#include "copico_hat_config.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "xbios_rom.h"

#include <cstdio>
#include <cstring>

extern void copico_bus_park_begin();
extern void copico_bus_park_end();

namespace {

uint8_t g_status_flags = FLASH_STATUS_SDC_MISSING;
uint8_t g_flash_result = COPICO_FLASH_RESULT_IDLE;
volatile uint8_t g_pending_flash_cmd = 0;

static const uint8_t kFujinetPlaceholder[16] = {
    0x7E, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

uint32_t slot_flash_offset(uint8_t slot) {
  return FLASH_ROM_BASE_OFFSET + static_cast<uint32_t>(slot) * FLASH_ROM_SLOT_SIZE;
}

uint16_t xor_checksum(const uint8_t* data, size_t size) {
  uint16_t sum = 0;
  for (size_t i = 0; i < size; ++i) {
    sum ^= data[i];
  }
  return sum;
}

bool slot_is_empty(uint8_t slot) {
  if (slot >= FLASH_ROM_MAX_SLOTS) {
    return true;
  }
  const auto* header =
      reinterpret_cast<const RomSlotHeader*>(XIP_BASE + slot_flash_offset(slot));
  return header->magic != FLASH_ROM_MAGIC;
}

void __not_in_flash_func(install_rom_parked)(uint8_t slot, const uint8_t* data, size_t size,
                                             uint16_t map_addr, const char* name,
                                             uint8_t version) {
  if (slot >= FLASH_ROM_MAX_SLOTS || !data || size == 0 ||
      size > (FLASH_ROM_SLOT_SIZE - 256)) {
    return;
  }

  const uint32_t flash_addr = slot_flash_offset(slot);
  RomSlotHeader header{};
  memset(&header, 0xFF, sizeof(header));
  header.magic = FLASH_ROM_MAGIC;
  header.size = static_cast<uint16_t>(size);
  header.map_addr = map_addr;
  header.checksum = xor_checksum(data, size);
  header.version = version;
  header.flags = (size > 8192) ? 0x01u : 0x00u;
  strncpy(header.name, name, sizeof(header.name) - 1);

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_addr, FLASH_ROM_SLOT_SIZE);

  uint8_t page_buf[256];
  memset(page_buf, 0xFF, sizeof(page_buf));
  memcpy(page_buf, &header, sizeof(header));
  flash_range_program(flash_addr, page_buf, 256);

  size_t offset = 0;
  while (offset < size) {
    size_t chunk = (size - offset > 256) ? 256 : (size - offset);
    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, data + offset, chunk);
    flash_range_program(flash_addr + 256 + offset, page_buf, 256);
    offset += chunk;
  }
  restore_interrupts(ints);

  const uint8_t* flash_ptr =
      reinterpret_cast<const uint8_t*>(XIP_BASE + flash_addr + 256);
  if (xor_checksum(flash_ptr, size) != header.checksum) {
    printf("[Flash] verify fail slot %u\n", slot);
    return;
  }
  printf("[Flash] installed '%s' in slot %u (%u bytes)\n", name, slot,
         static_cast<unsigned>(size));
}

void erase_slot_parked(uint8_t slot) {
  if (slot >= FLASH_ROM_MAX_SLOTS) {
    return;
  }
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(slot_flash_offset(slot), FLASH_ROM_SLOT_SIZE);
  restore_interrupts(ints);
  printf("[Flash] erased slot %u\n", slot);
}

void refresh_sdc_status_flag() {
  if (slot_is_empty(SLOT_COCOSDC)) {
    g_status_flags |= FLASH_STATUS_SDC_MISSING;
  } else {
    g_status_flags &= static_cast<uint8_t>(~FLASH_STATUS_SDC_MISSING);
  }
}

bool install_cocosdc_from_sd() {
  static uint8_t rom_buf[FLASH_ROM_SLOT_SIZE - 256];
  size_t rom_size = 0;

  static const char* k_rom_paths[] = {"/SDC-DOS.ROM", "/ROMS/SDC-DOS.ROM", nullptr};
  static const char* k_setup_paths[] = {"/SETUP.DSK", "/ROMS/SETUP.DSK", nullptr};

  bool loaded = false;
  for (const char** path = k_rom_paths; *path && !loaded; ++path) {
    loaded = copico_esp32_rom_fetch_file(*path, rom_buf, sizeof(rom_buf), &rom_size);
  }
  for (const char** path = k_setup_paths; *path && !loaded; ++path) {
    loaded = copico_esp32_rom_fetch_file(*path, rom_buf, sizeof(rom_buf), &rom_size);
  }

  if (!loaded) {
    printf("[Flash] SDC-DOS.ROM / SETUP.DSK not found on SD\n");
    g_status_flags |= FLASH_STATUS_SDC_MISSING;
    return false;
  }

  install_rom_parked(SLOT_COCOSDC, rom_buf, rom_size, 0xC000, "SDC-DOS", 1);
  install_rom_parked(copico_flash_bank_to_slot(0), rom_buf, rom_size, 0xC000, "SDC-DOS", 1);
  refresh_sdc_status_flag();
  return !slot_is_empty(SLOT_COCOSDC);
}

void clear_user_slots() {
  erase_slot_parked(SLOT_COCOSDC);
  for (uint8_t bank = 2; bank < BANK_COUNT; ++bank) {
    erase_slot_parked(copico_flash_bank_to_slot(bank));
  }
  g_status_flags |= FLASH_STATUS_SDC_MISSING;
}

void reinstall_factory_defaults() {
  install_rom_parked(SLOT_XBIOS, copico_xbios_bin, sizeof(copico_xbios_bin), 0xC000,
                     "CoPico X-BIOS", 1);
  install_rom_parked(SLOT_FUJINET, kFujinetPlaceholder, sizeof(kFujinetPlaceholder),
                     0xC000, "FujiNet (Pending)", 0);
  refresh_sdc_status_flag();
}

void run_pending_command(uint8_t cmd) {
  bool ok = false;
  switch (cmd) {
    case COPICO_FLASH_CMD_SCAN:
      ok = install_cocosdc_from_sd();
      break;
    case COPICO_FLASH_CMD_CLEAR:
      clear_user_slots();
      ok = true;
      break;
    case COPICO_FLASH_CMD_REINSTALL:
      reinstall_factory_defaults();
      ok = true;
      break;
    case COPICO_FLASH_CMD_INTERNAL:
      copico_config_set_pending_disk(XBIOS_DISK_INTERNAL);
      ok = true;
      break;
    default:
      ok = false;
      break;
  }
  g_flash_result = ok ? COPICO_FLASH_RESULT_OK : COPICO_FLASH_RESULT_ERROR;
}

}  // namespace

void copico_flash_rom_init() {
  copico_esp32_init();
  if (slot_is_empty(SLOT_XBIOS)) {
    install_rom_parked(SLOT_XBIOS, copico_xbios_bin, sizeof(copico_xbios_bin), 0xC000,
                       "CoPico X-BIOS", 1);
  }
  if (slot_is_empty(SLOT_FUJINET)) {
    install_rom_parked(SLOT_FUJINET, kFujinetPlaceholder, sizeof(kFujinetPlaceholder),
                       0xC000, "FujiNet (Pending)", 0);
  }
  refresh_sdc_status_flag();
  g_flash_result = COPICO_FLASH_RESULT_IDLE;
  g_pending_flash_cmd = 0;
  printf("[Flash] ROM bank base 0x%06lX, SDC-DOS %s\n",
         static_cast<unsigned long>(FLASH_ROM_BASE_OFFSET),
         (g_status_flags & FLASH_STATUS_SDC_MISSING) ? "missing" : "installed");
}

uint8_t copico_flash_status_flags() { return g_status_flags; }

uint8_t copico_flash_result() { return g_flash_result; }

bool copico_flash_queue_command(uint8_t cmd) {
  const bool valid = (cmd >= COPICO_FLASH_CMD_SCAN && cmd <= COPICO_FLASH_CMD_REINSTALL) ||
                     cmd == COPICO_FLASH_CMD_INTERNAL;
  if (!valid) {
    return false;
  }
  if (g_pending_flash_cmd != 0) {
    return false;
  }
  g_pending_flash_cmd = cmd;
  g_flash_result = COPICO_FLASH_RESULT_BUSY;
  return true;
}

bool copico_flash_command_pending() { return g_pending_flash_cmd != 0; }

void copico_flash_service_command() {
  if (!g_pending_flash_cmd) {
    return;
  }

  const uint8_t cmd = g_pending_flash_cmd;
  copico_bus_park_begin();
  run_pending_command(cmd);
  copico_bus_park_end();
  g_pending_flash_cmd = 0;
}

bool copico_flash_cart_read(uint16_t addr, uint8_t* out_byte) {
  if (!out_byte || addr < 0xC000) {
    return false;
  }

  if (addr == 0xD880) {
    *out_byte = g_status_flags;
    return true;
  }
  if (addr == 0xD881) {
    *out_byte = g_flash_result;
    return true;
  }

  return false;
}
