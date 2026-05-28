#pragma once

#include <Arduino.h>

// Toggle values — must match xbios.asm VAR_* / REG_* encoding
#define XBIOS_AUDIO_SPEECH   0
#define XBIOS_AUDIO_ORCH90   1
#define XBIOS_AUDIO_OFF      2

#define XBIOS_VIDEO_WORDPAK  0
#define XBIOS_VIDEO_SUPER    1
#define XBIOS_VIDEO_OFF      2

#define XBIOS_DISK_SDC       0
#define XBIOS_DISK_FUJINET   1
#define XBIOS_DISK_OFF       2
#define XBIOS_DISK_INTERNAL  3

#define XBIOS_COMM_WIMODEM   0
#define XBIOS_COMM_RS232     1
#define XBIOS_COMM_FUJINET   2
#define XBIOS_COMM_OFF       3

#define XBIOS_RTC_OFF        0
#define XBIOS_RTC_ON         1

#define HAT_EEPROM_MAGIC     0xC0
#define HAT_EEPROM_ADDR_MAGIC 0
#define HAT_EEPROM_ADDR_AUDIO 1
#define HAT_EEPROM_ADDR_VIDEO 2
#define HAT_EEPROM_ADDR_DISK  3
#define HAT_EEPROM_ADDR_COMM  4
#define HAT_EEPROM_ADDR_RTC   5

struct HatConfig {
    uint8_t audio = XBIOS_AUDIO_OFF;
    uint8_t video = XBIOS_VIDEO_OFF;
    uint8_t disk  = XBIOS_DISK_SDC;
    uint8_t comm  = XBIOS_COMM_OFF;
    uint8_t rtc   = XBIOS_RTC_OFF;
};

struct HatLayerFlags {
    bool boot_menu    = false;
    bool speech       = false;
    bool orch90       = false;
    bool wordpak      = false;
    bool cocosdc      = false;
    bool fujinet      = false;
    bool rs232        = false;
    bool wimodem      = false;
    bool internal_rom = false;
};

void hat_config_apply_constraints(HatConfig& cfg);
HatLayerFlags hat_config_to_layers(const HatConfig& cfg);
bool hat_config_is_valid(const HatConfig& cfg);
bool hat_config_load(HatConfig* out);
void hat_config_save(const HatConfig& cfg);
void hat_config_log(const HatConfig& cfg, const HatLayerFlags& layers);
