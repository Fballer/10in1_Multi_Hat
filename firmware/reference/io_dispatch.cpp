#include "io_dispatch.h"
#include "pic7040.h"
#include "acia6551.h"
#include "v9958.h"
#include "cocosdc.h"
#include "boot_menu.h"
#include "rtc.h"
#include "emulator_mode.h"

extern Pic7040   pic7040;
extern Acia6551  acia;
extern Acia6551  wimodem;
extern V9958     v9958;
extern Cocosdc   cocosdc;
extern BootMenu  boot_menu;
extern Rtc       rtc;
extern EmulatorMode current_mode;

IoReadFunc  io_read_table[256];
IoWriteFunc io_write_table[256];
RomReadFunc rom_read_handler = nullptr;

HatLayerFlags active_layers;

static uint8_t fujinet_rom_buffer[8192];

static uint8_t read_mode_register(uint16_t) {
    return (uint8_t)current_mode;
}

static uint8_t read_rtc(uint16_t addr) {
    return rtc.read(addr);
}

static void write_rtc(uint16_t addr, uint8_t data) {
    rtc.write(addr, data);
}

static uint8_t read_pic7040_status(uint16_t) {
    return pic7040.read_status();
}

static void write_pic7040_control(uint16_t, uint8_t data) {
    if (data & 0x01) pic7040.reset();
}

static void write_pic7040_data(uint16_t, uint8_t data) {
    pic7040.write_data(data);
}

static uint8_t read_acia_data(uint16_t)   { return acia.read_data(); }
static uint8_t read_acia_status(uint16_t) { return acia.read_status(); }
static void write_acia_data(uint16_t, uint8_t d)    { acia.write_data(d); }
static void write_acia_command(uint16_t, uint8_t d)  { acia.write_command(d); }
static void write_acia_control(uint16_t, uint8_t d)  { acia.write_control(d); }

static uint8_t read_wimodem_data(uint16_t)   { return wimodem.read_data(); }
static uint8_t read_wimodem_status(uint16_t) { return wimodem.read_status(); }
static void write_wimodem_data(uint16_t, uint8_t d)    { wimodem.write_data(d); }
static void write_wimodem_command(uint16_t, uint8_t d)  { wimodem.write_command(d); }
static void write_wimodem_control(uint16_t, uint8_t d)  { wimodem.write_control(d); }

static uint8_t read_v9958(uint16_t addr) { return v9958.read_port(addr); }
static void write_v9958(uint16_t addr, uint8_t data) { v9958.write_port(addr, data); }

static uint8_t read_cocosdc_reg(uint16_t addr) { return cocosdc.read_register(addr); }
static void write_cocosdc_reg(uint16_t addr, uint8_t data) { cocosdc.write_register(addr, data); }

extern uint8_t dac_left, dac_right;
extern void update_orch90_audio();

static void write_orch90_left(uint16_t, uint8_t data) {
    dac_left = data;
    update_orch90_audio();
}

static void write_orch90_right(uint16_t, uint8_t data) {
    dac_right = data;
    update_orch90_audio();
}

static uint8_t read_boot_rom(uint16_t addr) { return boot_menu.read_rom(addr); }
static uint8_t read_sdc_rom(uint16_t addr)  { return cocosdc.read_rom(addr); }

static uint8_t read_fujinet_rom(uint16_t addr) {
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        return fujinet_rom_buffer[addr - 0xC000];
    }
    return 0xFF;
}

static void install_cocosdc_io() {
    rom_read_handler = read_sdc_rom;
    io_read_table[0x40]  = read_cocosdc_reg;
    io_read_table[0x48]  = read_cocosdc_reg;
    io_read_table[0x49]  = read_cocosdc_reg;
    io_read_table[0x4A]  = read_cocosdc_reg;
    io_read_table[0x4B]  = read_cocosdc_reg;
    io_write_table[0x40] = write_cocosdc_reg;
    io_write_table[0x48] = write_cocosdc_reg;
    io_write_table[0x49] = write_cocosdc_reg;
    io_write_table[0x4A] = write_cocosdc_reg;
    io_write_table[0x4B] = write_cocosdc_reg;
}

static void install_speech_io() {
    io_read_table[0x7E]  = read_pic7040_status;
    io_write_table[0x7D] = write_pic7040_control;
    io_write_table[0x7E] = write_pic7040_data;
}

static void install_orch90_io() {
    io_write_table[0x7A] = write_orch90_left;
    io_write_table[0x7B] = write_orch90_right;
}

static void install_wordpak_io() {
    io_read_table[0x78]  = read_v9958;
    io_read_table[0x79]  = read_v9958;
    io_write_table[0x78] = write_v9958;
    io_write_table[0x79] = write_v9958;
    io_write_table[0x7A] = write_v9958;
    io_write_table[0x7B] = write_v9958;
}

static void install_rs232_io() {
    io_read_table[0x68]  = read_acia_data;
    io_read_table[0x69]  = read_acia_status;
    io_write_table[0x68] = write_acia_data;
    io_write_table[0x6A] = write_acia_command;
    io_write_table[0x6B] = write_acia_control;
}

static void install_wimodem_io() {
    io_read_table[0x68]  = read_wimodem_data;
    io_read_table[0x69]  = read_wimodem_status;
    io_write_table[0x68] = write_wimodem_data;
    io_write_table[0x6A] = write_wimodem_command;
    io_write_table[0x6B] = write_wimodem_control;
}

static void install_always_on_io() {
    io_read_table[0x7F]  = read_mode_register;
    io_read_table[0x50]  = read_rtc;
    io_write_table[0x51] = write_rtc;
    io_write_table[0x75] = write_rtc;
}

void rebuild_io_tables_boot_menu() {
    memset(io_read_table,  0, sizeof(io_read_table));
    memset(io_write_table, 0, sizeof(io_write_table));
    rom_read_handler = read_boot_rom;

    active_layers = HatLayerFlags{};
    active_layers.boot_menu = true;

    install_always_on_io();
}

void rebuild_io_tables_config(const HatConfig& cfg) {
    memset(io_read_table,  0, sizeof(io_read_table));
    memset(io_write_table, 0, sizeof(io_write_table));
    rom_read_handler = nullptr;

    active_layers = hat_config_to_layers(cfg);
    install_always_on_io();

    if (active_layers.internal_rom) {
        return;
    }

    // Disk ROM takes $C000 — CoCoSDC wins over FujiNet when both flagged.
    if (active_layers.cocosdc) {
        install_cocosdc_io();
    } else if (active_layers.fujinet) {
        rom_read_handler = read_fujinet_rom;
    }

    if (active_layers.speech) install_speech_io();
    if (active_layers.orch90) install_orch90_io();
    if (active_layers.wordpak) install_wordpak_io();
    if (active_layers.rs232) install_rs232_io();
    if (active_layers.wimodem) install_wimodem_io();
}

uint8_t* io_dispatch_fujinet_rom_buffer() {
    return fujinet_rom_buffer;
}
