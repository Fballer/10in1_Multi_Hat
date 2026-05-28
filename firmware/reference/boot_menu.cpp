#include "boot_menu.h"
#include <string.h>
#include "../bios/xbios_rom.h"
#include "flash_rom_manager.h"
#include "hat_config.h"

BootMenu::BootMenu() {
    memset(wifi_status, 0, sizeof(wifi_status));
    memset(fw_version, 0, sizeof(fw_version));
    memset(sd_status, 0, sizeof(sd_status));
    memset(tz_status, 0, sizeof(tz_status));
}

void BootMenu::init() {
}

void BootMenu::update_sys_info(const char* wifi, const char* fw, const char* sd, const char* tz) {
    strncpy(wifi_status, wifi, sizeof(wifi_status) - 1);
    strncpy(fw_version, fw, sizeof(fw_version) - 1);
    strncpy(sd_status, sd, sizeof(sd_status) - 1);
    strncpy(tz_status, tz, sizeof(tz_status) - 1);
}

uint8_t BootMenu::read_rom(uint16_t address) {
    uint16_t offset;
    if (address >= 0xC000 && address <= 0xDFFF) {
        offset = address - 0xC000;
    } else if (address >= 0xFFFE) {
        // CoCo reset vectors alias the last two bytes of the 8K ROM image.
        offset = 0x1FFE + (address - 0xFFFE);
    } else {
        return 0xFF;
    }

    if (address >= 0xD800 && address < 0xD820) {
        return wifi_status[address - 0xD800];
    }
    if (address >= 0xD820 && address < 0xD840) {
        return fw_version[address - 0xD820];
    }
    if (address >= 0xD840 && address < 0xD860) {
        return sd_status[address - 0xD840];
    }
    if (address >= 0xD860 && address < 0xD880) {
        return tz_status[address - 0xD860];
    }
    if (address == 0xD880) {
        return flash_rom.status_flags;
    }
    if (address == 0xD881) {
        return flash_result;
    }

    if (offset >= copico_xbios_bin_len) {
        return 0xFF;
    }
    return copico_xbios_bin[offset];
}

void BootMenu::set_config(uint16_t address, uint8_t data) {
    if (address == 0xFF70) reg_audio = data;
    else if (address == 0xFF71) reg_video = data;
    else if (address == 0xFF72) reg_disk = data;
    else if (address == 0xFF73) reg_comm = data;
    else if (address == 0xFF74) reg_rtc = data;

    HatConfig scratch;
    scratch.audio = reg_audio;
    scratch.video = reg_video;
    scratch.disk = reg_disk;
    scratch.comm = reg_comm;
    scratch.rtc = reg_rtc;
    hat_config_apply_constraints(scratch);
    reg_audio = scratch.audio;
    reg_video = scratch.video;
    reg_disk = scratch.disk;
    reg_comm = scratch.comm;
}

void BootMenu::set_flash_command(uint8_t cmd) {
    if ((cmd >= FLASH_CMD_SCAN_SD && cmd <= FLASH_CMD_REINSTALL) ||
        cmd == FLASH_CMD_INTERNAL_ROM) {
        pending_flash_cmd = cmd;
        flash_result = FLASH_RESULT_BUSY;
    }
}

void BootMenu::run_flash_command(uint8_t cmd) {
    bool ok = false;

    switch (cmd) {
        case FLASH_CMD_SCAN_SD:
            ok = flash_rom.install_cocosdc_from_sd();
            break;
        case FLASH_CMD_CLEAR_BANK:
            flash_rom.clear_user_slots();
            ok = true;
            break;
        case FLASH_CMD_REINSTALL:
            flash_rom.init_factory_defaults(true);
            if (flash_rom.is_slot_empty(SLOT_COCOSDC)) {
                flash_rom.status_flags |= FLASH_STATUS_SDC_MISSING;
            }
            ok = true;
            break;
        case FLASH_CMD_INTERNAL_ROM:
            reg_disk = XBIOS_DISK_INTERNAL;
            pending_config_apply = true;
            ok = true;
            break;
        default:
            break;
    }

    flash_result = ok ? FLASH_RESULT_OK : FLASH_RESULT_ERROR;
    pending_flash_cmd = 0;
}

bool BootMenu::get_config(HatConfig* out) const {
    if (!out) return false;
    out->audio = reg_audio;
    out->video = reg_video;
    out->disk = reg_disk;
    out->comm = reg_comm;
    out->rtc = reg_rtc;
    HatConfig adjusted = *out;
    hat_config_apply_constraints(adjusted);
    *out = adjusted;
    return true;
}

bool BootMenu::consume_pending_config_apply(HatConfig* out) {
    if (!pending_config_apply || !out) return false;
    get_config(out);
    pending_config_apply = false;
    return true;
}

void BootMenu::service_flash_command() {
    if (pending_flash_cmd != 0 && flash_result == FLASH_RESULT_BUSY) {
        run_flash_command(pending_flash_cmd);
    }
}
