#ifndef BOOT_MENU_H
#define BOOT_MENU_H

#include <Arduino.h>
#include "hat_config.h"

#define FLASH_CMD_SCAN_SD     1
#define FLASH_CMD_CLEAR_BANK  2
#define FLASH_CMD_REINSTALL   3
#define FLASH_CMD_INTERNAL_ROM 9

#define FLASH_RESULT_IDLE     0
#define FLASH_RESULT_BUSY     1
#define FLASH_RESULT_OK       2
#define FLASH_RESULT_ERROR    3

class BootMenu {
public:
    BootMenu();
    void init();
    uint8_t read_rom(uint16_t address);
    void update_sys_info(const char* wifi, const char* fw, const char* sd, const char* tz);
    void set_config(uint16_t address, uint8_t data);
    void set_flash_command(uint8_t cmd);
    void service_flash_command();
    bool get_config(HatConfig* out) const;
    bool consume_pending_config_apply(HatConfig* out);

    uint8_t flash_result = FLASH_RESULT_IDLE;

private:
    void run_flash_command(uint8_t cmd);

    char wifi_status[32];
    char fw_version[32];
    char sd_status[32];
    char tz_status[32];

    uint8_t reg_disk = 0;
    uint8_t reg_comm = 0;
    uint8_t reg_audio = 0;
    uint8_t reg_video = 0;
    uint8_t reg_rtc = 0;

    uint8_t pending_flash_cmd = 0;
    bool pending_config_apply = false;
};

#endif // BOOT_MENU_H
