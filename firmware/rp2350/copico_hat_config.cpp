#include "copico_hat_config.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <cstring>

namespace {

constexpr uint32_t kConfigFlashOffsets[] = {
    PICO_FLASH_SIZE_BYTES - (2 * FLASH_SECTOR_SIZE),
    PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE,
};

struct StoredConfig {
  uint8_t magic;
  uint8_t audio;
  uint8_t video;
  uint8_t disk;
  uint8_t comm;
  uint8_t rtc;
  uint8_t tz;
};

HatConfig g_active;
HatConfig g_pending;
bool g_boot_menu_active = true;
volatile bool g_flash_pending = false;
volatile uint8_t g_save_status = COPICO_SAVE_STATUS_IDLE;
volatile bool g_reboot_requested = false;

bool config_is_valid(const HatConfig& cfg) {
  if (cfg.audio > XBIOS_AUDIO_OFF) return false;
  if (cfg.video > XBIOS_VIDEO_OFF) return false;
  if (cfg.disk > XBIOS_DISK_INTERNAL) return false;
  if (cfg.comm > XBIOS_COMM_OFF) return false;
  if (cfg.rtc > XBIOS_RTC_ON) return false;
  if (cfg.tz > XBIOS_TZ_MAX) return false;
  return true;
}

void apply_constraints(HatConfig& cfg) {
  if (cfg.video == XBIOS_VIDEO_WORDPAK && cfg.audio == XBIOS_AUDIO_ORCH90) {
    cfg.audio = XBIOS_AUDIO_OFF;
  }
  if (cfg.disk == XBIOS_DISK_FUJINET) {
    cfg.comm = XBIOS_COMM_FUJINET;
  } else if (cfg.comm == XBIOS_COMM_FUJINET) {
    cfg.disk = XBIOS_DISK_FUJINET;
  }
}

const StoredConfig* find_stored_config() {
  for (uint32_t offset : kConfigFlashOffsets) {
    const auto* stored =
        reinterpret_cast<const StoredConfig*>(XIP_BASE + offset);
    if (stored->magic == HAT_EEPROM_MAGIC) {
      return stored;
    }
  }
  return nullptr;
}

void load_from_flash() {
  const StoredConfig* stored = find_stored_config();
  if (!stored) {
    g_active = HatConfig{};
    g_pending = g_active;
    return;
  }

  HatConfig loaded{};
  loaded.audio = stored->audio;
  loaded.video = stored->video;
  loaded.disk = stored->disk;
  loaded.comm = stored->comm;
  loaded.rtc = stored->rtc;
  loaded.tz = (stored->tz <= XBIOS_TZ_MAX) ? stored->tz : 0;

  if (!config_is_valid(loaded)) {
    g_active = HatConfig{};
    g_pending = g_active;
    return;
  }

  apply_constraints(loaded);
  g_active = loaded;
  g_pending = loaded;
}

void __not_in_flash_func(write_to_flash)(const HatConfig& cfg) {
  StoredConfig stored{};
  stored.magic = HAT_EEPROM_MAGIC;
  stored.audio = cfg.audio;
  stored.video = cfg.video;
  stored.disk = cfg.disk;
  stored.comm = cfg.comm;
  stored.rtc = cfg.rtc;
  stored.tz = cfg.tz;

  uint8_t sector[FLASH_SECTOR_SIZE];
  memset(sector, 0xFF, sizeof(sector));
  memcpy(sector, &stored, sizeof(stored));

  const uint32_t offset = kConfigFlashOffsets[0];

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(offset, FLASH_SECTOR_SIZE);
  flash_range_program(offset, sector, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);
}

void queue_flash_save() {
  apply_constraints(g_pending);
  g_active = g_pending;
  g_flash_pending = true;
  g_save_status = COPICO_SAVE_STATUS_PENDING;
}

}  // namespace

void copico_config_init() { load_from_flash(); }

bool copico_boot_menu_active() { return g_boot_menu_active; }

void copico_set_boot_menu_active(bool active) { g_boot_menu_active = active; }

void copico_config_exit_boot_menu() {
  g_boot_menu_active = false;
  g_pending = g_active;
}

uint8_t copico_reg_read(uint16_t addr) {
  // During the boot menu, reads reflect the in-progress selection (pending).
  // After boot, reads return the last committed configuration.
  const HatConfig& cfg =
      copico_boot_menu_active() ? g_pending : g_active;
  switch (addr) {
    case 0xFF70:
      return cfg.audio;
    case 0xFF71:
      return cfg.video;
    case 0xFF72:
      return cfg.disk;
    case 0xFF73:
      return cfg.comm;
    case 0xFF74:
      return cfg.rtc;
    case 0xFF75:
      return cfg.tz;
    case 0xFF7E:
      return g_save_status;
    default:
      return 0xFF;
  }
}

bool copico_reg_write(uint16_t addr, uint8_t data) {
  switch (addr) {
    case 0xFF70:
      g_pending.audio = data;
      break;
    case 0xFF71:
      g_pending.video = data;
      break;
    case 0xFF72:
      g_pending.disk = data;
      break;
    case 0xFF73:
      g_pending.comm = data;
      break;
    case 0xFF74:
      g_pending.rtc = data;
      break;
    case 0xFF75:
      if (data > XBIOS_TZ_MAX) {
        return false;
      }
      g_pending.tz = data;
      return false;
    case 0xFF7F:
      if (data == COPICO_BOOT_MENU) {
        g_boot_menu_active = true;
        g_pending = g_active;
        g_save_status = COPICO_SAVE_STATUS_IDLE;
      } else if (data == COPICO_BOOT_APPLY) {
        // Explicit save trigger is VAR_SAVE ($0106), not REG_BOOT.
        // Keep boot-menu context unchanged here to avoid stale readback during
        // the saving summary page.
      }
      return false;
    default:
      break;
  }

  apply_constraints(g_pending);
  return false;
}

bool copico_config_flash_pending() { return g_flash_pending; }

void __not_in_flash_func(copico_config_perform_flash_commit)() {
  if (!g_flash_pending) {
    return;
  }

  g_flash_pending = false;
  g_save_status = COPICO_SAVE_STATUS_PENDING;
  write_to_flash(g_active);
  g_pending = g_active;
  g_save_status = COPICO_SAVE_STATUS_OK;
}

bool copico_config_take_reboot_request() {
  if (!g_reboot_requested) {
    return false;
  }
  g_reboot_requested = false;
  return true;
}

bool copico_config_on_menu_poke(uint16_t addr, uint8_t data, uint8_t* coco_ram) {
  (void)coco_ram;
  if (addr != COPICO_MENU_VAR_SAVE_ADDR || data != COPICO_SAVE_MAGIC) {
    return false;
  }
  g_reboot_requested = true;
  queue_flash_save();
  return true;
}

void copico_config_save() {
  queue_flash_save();
  copico_config_perform_flash_commit();
}

const HatConfig& copico_config_current() { return g_active; }
