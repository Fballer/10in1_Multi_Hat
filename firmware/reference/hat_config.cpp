#include "hat_config.h"
#include <EEPROM.h>
#include "emulator_mode.h"

void hat_config_apply_constraints(HatConfig& cfg) {
    // Orch90 ($FF7A/B) conflicts with WordPak V9958 — WordPak wins.
    if (cfg.video == XBIOS_VIDEO_WORDPAK && cfg.audio == XBIOS_AUDIO_ORCH90) {
        cfg.audio = XBIOS_AUDIO_OFF;
    }

    // FujiNet disk/comm coupling (matches xbios.asm DO_DISK / DO_COMM).
    if (cfg.disk == XBIOS_DISK_FUJINET) {
        cfg.comm = XBIOS_COMM_FUJINET;
    } else if (cfg.comm == XBIOS_COMM_FUJINET) {
        cfg.disk = XBIOS_DISK_FUJINET;
    }
}

bool hat_config_is_valid(const HatConfig& cfg) {
    if (cfg.audio > XBIOS_AUDIO_OFF) return false;
    if (cfg.video > XBIOS_VIDEO_OFF) return false;
    if (cfg.disk > XBIOS_DISK_INTERNAL) return false;
    if (cfg.comm > XBIOS_COMM_OFF) return false;
    if (cfg.rtc > XBIOS_RTC_ON) return false;
    return true;
}

HatLayerFlags hat_config_to_layers(const HatConfig& cfg) {
    HatConfig c = cfg;
    hat_config_apply_constraints(c);

    HatLayerFlags layers;
    layers.boot_menu = false;

    if (c.disk == XBIOS_DISK_INTERNAL) {
        layers.internal_rom = true;
        return layers;
    }

    if (c.audio == XBIOS_AUDIO_SPEECH) layers.speech = true;
    if (c.audio == XBIOS_AUDIO_ORCH90) layers.orch90 = true;

    if (c.video == XBIOS_VIDEO_WORDPAK) layers.wordpak = true;

    if (c.disk == XBIOS_DISK_SDC) layers.cocosdc = true;
    if (c.disk == XBIOS_DISK_FUJINET || c.comm == XBIOS_COMM_FUJINET) {
        layers.fujinet = true;
    }

    if (c.comm == XBIOS_COMM_RS232) layers.rs232 = true;
    if (c.comm == XBIOS_COMM_WIMODEM) layers.wimodem = true;

    return layers;
}

static void migrate_legacy_mode(uint8_t legacy_mode, HatConfig* out) {
    *out = HatConfig{};
    switch ((EmulatorMode)legacy_mode) {
        case MODE_ORCH90:
            out->audio = XBIOS_AUDIO_ORCH90;
            break;
        case MODE_SPEECH_SOUND:
            out->audio = XBIOS_AUDIO_SPEECH;
            break;
        case MODE_WORDPAK2:
            out->video = XBIOS_VIDEO_WORDPAK;
            break;
        case MODE_COCOSDC:
            out->disk = XBIOS_DISK_SDC;
            break;
        case MODE_FUJINET:
            out->disk = XBIOS_DISK_FUJINET;
            out->comm = XBIOS_COMM_FUJINET;
            break;
        case MODE_RS232_PAK_LEGACY:
        case MODE_RS232_PAK_TURBO:
            out->comm = XBIOS_COMM_RS232;
            break;
        case MODE_WIMODEM:
            out->comm = XBIOS_COMM_WIMODEM;
            break;
        case MODE_INTERNAL_ROM:
            out->disk = XBIOS_DISK_INTERNAL;
            break;
        default:
            out->disk = XBIOS_DISK_SDC;
            break;
    }
}

bool hat_config_load(HatConfig* out) {
    if (!out) return false;

    if (EEPROM.read(HAT_EEPROM_ADDR_MAGIC) == HAT_EEPROM_MAGIC) {
        out->audio = EEPROM.read(HAT_EEPROM_ADDR_AUDIO);
        out->video = EEPROM.read(HAT_EEPROM_ADDR_VIDEO);
        out->disk  = EEPROM.read(HAT_EEPROM_ADDR_DISK);
        out->comm  = EEPROM.read(HAT_EEPROM_ADDR_COMM);
        out->rtc   = EEPROM.read(HAT_EEPROM_ADDR_RTC);
        if (hat_config_is_valid(*out)) {
            hat_config_apply_constraints(*out);
            return true;
        }
    }

    uint8_t legacy = EEPROM.read(0);
    if (legacy > 0 && legacy < MODE_MAX && legacy != MODE_BOOT_MENU) {
        migrate_legacy_mode(legacy, out);
        hat_config_apply_constraints(*out);
        hat_config_save(*out);
        return true;
    }

    *out = HatConfig{};
    return false;
}

void hat_config_save(const HatConfig& cfg) {
    HatConfig c = cfg;
    hat_config_apply_constraints(c);
    EEPROM.write(HAT_EEPROM_ADDR_MAGIC, HAT_EEPROM_MAGIC);
    EEPROM.write(HAT_EEPROM_ADDR_AUDIO, c.audio);
    EEPROM.write(HAT_EEPROM_ADDR_VIDEO, c.video);
    EEPROM.write(HAT_EEPROM_ADDR_DISK, c.disk);
    EEPROM.write(HAT_EEPROM_ADDR_COMM, c.comm);
    EEPROM.write(HAT_EEPROM_ADDR_RTC, c.rtc);
    EEPROM.commit();
}

void hat_config_log(const HatConfig& cfg, const HatLayerFlags& layers) {
    Serial.print("[Config] audio=");
    Serial.print(cfg.audio);
    Serial.print(" video=");
    Serial.print(cfg.video);
    Serial.print(" disk=");
    Serial.print(cfg.disk);
    Serial.print(" comm=");
    Serial.print(cfg.comm);
    Serial.print(" rtc=");
    Serial.println(cfg.rtc);

    Serial.print("[Layers]");
    if (layers.speech) Serial.print(" speech");
    if (layers.orch90) Serial.print(" orch90");
    if (layers.wordpak) Serial.print(" wordpak");
    if (layers.cocosdc) Serial.print(" cocosdc");
    if (layers.fujinet) Serial.print(" fujinet");
    if (layers.rs232) Serial.print(" rs232");
    if (layers.wimodem) Serial.print(" wimodem");
    if (layers.internal_rom) Serial.print(" internal_rom");
    if (layers.boot_menu) Serial.print(" boot_menu");
    Serial.println();
}
