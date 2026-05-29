#pragma once

#include <cstdint>

// Toggle values — must match xbios.asm VAR_* / REG_* encoding
#define XBIOS_AUDIO_SPEECH 0
#define XBIOS_AUDIO_ORCH90 1
#define XBIOS_AUDIO_OFF 2

#define XBIOS_VIDEO_WORDPAK 0
#define XBIOS_VIDEO_SUPER 1
#define XBIOS_VIDEO_OFF 2

#define XBIOS_DISK_SDC 0
#define XBIOS_DISK_FUJINET 1
#define XBIOS_DISK_OFF 2
#define XBIOS_DISK_INTERNAL 3

#define XBIOS_COMM_WIMODEM 0
#define XBIOS_COMM_RS232 1
#define XBIOS_COMM_OFF 2

#define XBIOS_RTC_OFF 0
#define XBIOS_RTC_ON 1
#define XBIOS_TZ_MIN 0
#define XBIOS_TZ_MAX 17

#define COPICO_BOOT_APPLY 0x55
#define COPICO_BOOT_MENU 0
#define COPICO_SAVE_MAGIC 0xA5
#define COPICO_MENU_VAR_SAVE_ADDR 0x0106
#define COPICO_SAVE_STATUS_IDLE 0
#define COPICO_SAVE_STATUS_PENDING 1
#define COPICO_SAVE_STATUS_OK 2
#define COPICO_SAVE_STATUS_ERR 3

#define HAT_EEPROM_MAGIC 0xC0

struct HatConfig {
  uint8_t audio = XBIOS_AUDIO_OFF;
  uint8_t video = XBIOS_VIDEO_OFF;
  uint8_t disk = XBIOS_DISK_OFF;
  uint8_t comm = XBIOS_COMM_OFF;
  uint8_t rtc = XBIOS_RTC_OFF;
  uint8_t tz = 0;
};

void copico_config_init();
bool copico_config_on_menu_poke(uint16_t addr, uint8_t data, uint8_t* coco_ram);
bool copico_config_flash_pending();
void copico_config_perform_flash_commit();
bool copico_config_take_reboot_request();
bool copico_boot_menu_active();
void copico_set_boot_menu_active(bool active);
void copico_config_exit_boot_menu();

uint8_t copico_reg_read(uint16_t addr);
bool copico_reg_write(uint16_t addr, uint8_t data);

void copico_config_save();
void copico_config_set_pending_disk(uint8_t disk);
const HatConfig& copico_config_current();
