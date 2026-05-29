#pragma once
#include <Arduino.h>
#include "hardware/flash.h"
#include "emulator_mode.h"

// ================================================================
// Flash ROM Bank Architecture
// ================================================================
// The RP2350 flash is 4MB. We reserve 256KB at the very end for
// ROM storage, safely above the firmware code (~256KB max).
//
// Layout (each slot = 16KB = 16384 bytes):
//
//   System Slots (pre-loaded from firmware or SD):
//     Slot 0: CoPico X-BIOS   (Embedded - user's own code)
//     Slot 1: SDC-DOS         (Auto-install from /ROMS/SDC-DOS.ROM or SETUP.DSK)
//     Slot 2: FujiNet BIOS    (Embedded - open source)
//     Slot 3: RS-232 Pak ROM  (Embedded - abandonware)
//
//   CoCoSDC Bank Array (emulates the real CoCoSDC 8-bank hardware):
//     Bank 0: SDC-DOS         (Mirror of Slot 1, set on install)
//     Bank 1: Disk BASIC 1.1  (Embedded - same precedent as CoCoSDC)
//     Bank 2-7: User Slots    (Empty, filled via ROM Utility Menu)
//
// Total: 4 System Slots + 8 Bank Slots = 12 slots x 16KB = 192KB
// Base Address: 4MB - 256KB = 0x3C0000 (flash offset, not XIP address)
// ================================================================

#define FLASH_TOTAL_SIZE        (4 * 1024 * 1024)  // 4MB
#define FLASH_ROM_SLOT_SIZE     (16 * 1024)          // 16KB per slot
#define FLASH_ROM_RESERVED      (256 * 1024)         // 256KB total
#define FLASH_ROM_BASE_OFFSET   (FLASH_TOTAL_SIZE - FLASH_ROM_RESERVED)
#define FLASH_ROM_MAX_SLOTS     12

// Slot index constants
#define SLOT_XBIOS      0   // CoPico X-BIOS Boot Menu BIOS
#define SLOT_COCOSDC    1   // SDC-DOS (user supplied via SD)
#define SLOT_FUJINET    2   // FujiNet BIOS (open source, embedded)
#define SLOT_RS232      3   // RS-232 Pak ROM (abandonware, embedded)

// SDC Bank Array starts at slot 4
#define BANK_SDC_BASE   4   // Bank 0 = Slot 4, Bank 1 = Slot 5, etc.
#define BANK_COUNT      8

// Magic number embedded at the start of each valid slot header
#define FLASH_ROM_MAGIC  0xC0C05DC1

// ================================================================
// Slot Header (stored at the beginning of each 16KB slot)
// We reserve 256 bytes for the header so ROM data starts at +256.
// ================================================================
struct RomSlotHeader {
    uint32_t magic;         // 0xC0C05DC1 if slot is valid
    uint16_t size;          // Actual ROM size (8192 or 16384)
    uint16_t map_addr;      // CoCo bus address to map at ($C000 or $8000)
    uint16_t checksum;      // XOR checksum of ROM data
    uint8_t  version;       // ROM version (for update checking)
    uint8_t  flags;         // Bit 0: is_16k, Bit 1: needs_sd_install
    char     name[20];      // Human-readable name for Boot Menu display
    uint8_t  _reserved[224]; // Pad to exactly 256 bytes
};
static_assert(sizeof(RomSlotHeader) == 256, "RomSlotHeader must be exactly 256 bytes");

// Status flags for the SDC-DOS missing alert (shared with Chameleon BIOS at $D880)
#define FLASH_STATUS_SDC_MISSING    0x01
#define FLASH_STATUS_SDC_INSTALLED  0x00

class FlashRomManager {
public:
    FlashRomManager();

    // Call on every boot to ensure factory ROMs are in their slots.
    // Does NOT touch SDC-DOS (Slot 1) unless force_reinstall is true.
    void init_factory_defaults(bool force_reinstall = false);

    // Erase slot header (marks empty). Does not erase full 16KB on RP2350
    // (flash erase is done on next install_rom).
    void erase_slot(uint8_t slot);

    // Clear user-supplied ROMs (SDC-DOS + empty CoCoSDC bank slots).
    void clear_user_slots();

    // Try to load /SDC-DOS.ROM or official /SETUP.DSK from SD via ESP32 into Slot 1.
    bool install_cocosdc_from_sd();

    // Install a ROM into a slot from a data buffer.
    // Erases the slot first, then writes header + data.
    // MUST be called with Core 1 suspended (handled internally).
    bool install_rom(uint8_t slot, const uint8_t* data, size_t size,
                     uint16_t map_addr, const char* name, uint8_t version = 1);

    // Copy ROM data from flash slot into a RAM buffer.
    // Returns false if slot is empty or checksum fails.
    bool load_rom_to_buffer(uint8_t slot, uint8_t* buffer, uint16_t* out_size = nullptr, uint16_t* out_map_addr = nullptr);

    // Returns true if a slot has no valid ROM installed.
    bool is_slot_empty(uint8_t slot);

    // Returns the slot header info (for display in Boot Menu).
    bool get_slot_info(uint8_t slot, RomSlotHeader* out_header);

    // Convert a CoCoSDC bank number (0-7) to the corresponding flash slot index.
    static uint8_t bank_to_slot(uint8_t bank) { return BANK_SDC_BASE + bank; }

    // Status byte returned to Chameleon BIOS at $D880 address
    uint8_t status_flags = 0;

private:
    uint32_t get_flash_addr(uint8_t slot);
    uint16_t calc_checksum(const uint8_t* data, size_t size);
};

extern FlashRomManager flash_rom;
