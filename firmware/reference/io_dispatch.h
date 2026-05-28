#pragma once
// ================================================================
// Hybrid Fast-Map I/O Dispatch — layered hat configuration
// ================================================================

#include <Arduino.h>
#include "hat_config.h"

typedef uint8_t (*IoReadFunc)(uint16_t addr);
typedef void    (*IoWriteFunc)(uint16_t addr, uint8_t data);
typedef uint8_t (*RomReadFunc)(uint16_t addr);

extern IoReadFunc  io_read_table[256];
extern IoWriteFunc io_write_table[256];
extern RomReadFunc rom_read_handler;

extern HatLayerFlags active_layers;

void rebuild_io_tables_boot_menu();
void rebuild_io_tables_config(const HatConfig& cfg);
uint8_t* io_dispatch_fujinet_rom_buffer();
