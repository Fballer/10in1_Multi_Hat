// ================================================================
// Flash ROM Manager — Copico 10-in-1 Master Hat
// ================================================================
// Manages persistent ROM storage in the RP2350's internal flash.
// See flash_rom_manager.h for full layout documentation.
// ================================================================

#include "flash_rom_manager.h"
#include "esp32_bridge.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "../bios/xbios_rom.h"   // auto-generated from xbios.asm
#include <string.h>

FlashRomManager flash_rom;

// ================================================================
// Embedded ROM Data — Pre-loaded into Flash on first boot.
// These are abandonware ROMs redistributed freely throughout the
// CoCo community. The CoCoSDC itself ships with Disk BASIC 1.1.
// ================================================================

// Disk BASIC 1.1 (SDC Bank 1) — 16KB
// This is the same ROM pre-loaded on the original CoCoSDC hardware.
// Sourced from the Color Computer Archive (colorcomputerarchive.com).
// Tandy/RadioShack is defunct; IP was not carried forward actively.
extern const uint8_t disk_basic_11_bin[];
extern const unsigned int disk_basic_11_bin_len;

// RS-232 Deluxe Pak (Slot 3) — 8KB
// Tandy 26-2226 ROM, preserved at colorcomputerarchive.com.
// Enables CoCo PRINT#-1 and standard serial driver functionality.
extern const uint8_t rs232_pak_bin[];
extern const unsigned int rs232_pak_bin_len;

// FujiNet BIOS is open-source — included directly from the FujiNet project.
// For now we placeholder this; the real binary will be downloaded by the ESP32.
// When available, it will be embedded similarly to the others.
static const uint8_t FUJINET_PLACEHOLDER[16] = {
    // 6809: JMP [$FFFE] — gracefully falls through to internal ROM
    0x7E, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ================================================================
// FlashRomManager Implementation
// ================================================================

FlashRomManager::FlashRomManager() {
    status_flags = 0;
}

uint32_t FlashRomManager::get_flash_addr(uint8_t slot) {
    return FLASH_ROM_BASE_OFFSET + ((uint32_t)slot * FLASH_ROM_SLOT_SIZE);
}

uint16_t FlashRomManager::calc_checksum(const uint8_t* data, size_t size) {
    uint16_t xor_sum = 0;
    for (size_t i = 0; i < size; i++) {
        xor_sum ^= data[i];
    }
    return xor_sum;
}

bool FlashRomManager::get_slot_info(uint8_t slot, RomSlotHeader* out_header) {
    if (slot >= FLASH_ROM_MAX_SLOTS || !out_header) return false;
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + get_flash_addr(slot));
    memcpy(out_header, flash_ptr, sizeof(RomSlotHeader));
    return (out_header->magic == FLASH_ROM_MAGIC);
}

bool FlashRomManager::is_slot_empty(uint8_t slot) {
    if (slot >= FLASH_ROM_MAX_SLOTS) return true;
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + get_flash_addr(slot));
    const RomSlotHeader* header = (const RomSlotHeader*)flash_ptr;
    return (header->magic != FLASH_ROM_MAGIC);
}

bool FlashRomManager::install_rom(uint8_t slot, const uint8_t* data, size_t size,
                                   uint16_t map_addr, const char* name, uint8_t version) {
    if (slot >= FLASH_ROM_MAX_SLOTS) return false;
    if (size > (FLASH_ROM_SLOT_SIZE - 256)) return false; // Must fit after header
    if (!data || size == 0) return false;

    uint32_t flash_addr = get_flash_addr(slot);

    // Build the header
    RomSlotHeader header;
    memset(&header, 0xFF, sizeof(header)); // 0xFF = erased flash default
    header.magic    = FLASH_ROM_MAGIC;
    header.size     = (uint16_t)size;
    header.map_addr = map_addr;
    header.checksum = calc_checksum(data, size);
    header.version  = version;
    header.flags    = (size > 8192) ? 0x01 : 0x00; // Bit 0 = is_16k
    strncpy(header.name, name, sizeof(header.name) - 1);

    // ---------------------------------------------------------------
    // CRITICAL: Flash writes require:
    //   1. Core 1 suspended (it runs the bus handler)
    //   2. Interrupts disabled
    //   3. XIP cache flushed after writing
    // ---------------------------------------------------------------
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();

    // Erase the full 16KB slot (4 x 4KB sectors)
    flash_range_erase(flash_addr, FLASH_ROM_SLOT_SIZE);

    // Write header (256 bytes, padded to page boundary)
    uint8_t page_buf[256];
    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, &header, sizeof(RomSlotHeader));
    flash_range_program(flash_addr, page_buf, 256);

    // Write ROM data in 256-byte pages
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = (size - offset > 256) ? 256 : (size - offset);
        memset(page_buf, 0xFF, sizeof(page_buf));
        memcpy(page_buf, data + offset, chunk);
        flash_range_program(flash_addr + 256 + offset, page_buf, 256);
        offset += chunk;
    }

    restore_interrupts(ints);
    multicore_lockout_end_blocking();

    // Verify the write
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + flash_addr + 256);
    uint16_t verify_csum = calc_checksum(flash_ptr, size);
    if (verify_csum != header.checksum) {
        Serial.print("[Flash] VERIFY FAIL slot=");
        Serial.println(slot);
        return false;
    }

    Serial.print("[Flash] Installed '");
    Serial.print(name);
    Serial.print("' to slot ");
    Serial.println(slot);
    return true;
}

bool FlashRomManager::load_rom_to_buffer(uint8_t slot, uint8_t* buffer,
                                          uint16_t* out_size, uint16_t* out_map_addr) {
    if (slot >= FLASH_ROM_MAX_SLOTS || !buffer) return false;

    uint32_t flash_addr = get_flash_addr(slot);
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + flash_addr);
    const RomSlotHeader* header = (const RomSlotHeader*)flash_ptr;

    if (header->magic != FLASH_ROM_MAGIC) return false;
    if (header->size == 0 || header->size > (FLASH_ROM_SLOT_SIZE - 256)) return false;

    // Verify checksum before loading
    const uint8_t* rom_data = flash_ptr + 256;
    uint16_t csum = calc_checksum(rom_data, header->size);
    if (csum != header->checksum) {
        Serial.print("[Flash] Checksum mismatch slot=");
        Serial.println(slot);
        return false;
    }

    memcpy(buffer, rom_data, header->size);
    if (out_size)     *out_size     = header->size;
    if (out_map_addr) *out_map_addr = header->map_addr;
    return true;
}

// ================================================================
// CoPico X-BIOS Placeholder (Slot 0)
// ================================================================
// Called once per boot. Ensures embedded ROMs are in their slots.
// DOES NOT touch Slot 1 (SDC-DOS) — that is user-supplied via SD.
// DOES NOT re-flash if the version matches what is already there.
// ================================================================
void FlashRomManager::erase_slot(uint8_t slot) {
    if (slot >= FLASH_ROM_MAX_SLOTS) return;
    uint32_t flash_addr = get_flash_addr(slot);
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_addr, FLASH_ROM_SLOT_SIZE);
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    Serial.print("[Flash] Erased slot ");
    Serial.println(slot);
}

void FlashRomManager::clear_user_slots() {
    erase_slot(SLOT_COCOSDC);
    for (uint8_t bank = 2; bank < BANK_COUNT; bank++) {
        erase_slot(bank_to_slot(bank));
    }
    status_flags |= FLASH_STATUS_SDC_MISSING;
}

bool FlashRomManager::install_cocosdc_from_sd() {
    static uint8_t rom_buf[FLASH_ROM_SLOT_SIZE - 256];
    size_t rom_size = 0;

    if (!esp32.rom_fetch_file("/COCOSDC.ROM", rom_buf, sizeof(rom_buf), &rom_size)) {
        if (!esp32.rom_fetch_file("/ROMS/COCOSDC.ROM", rom_buf, sizeof(rom_buf), &rom_size)) {
            Serial.println("[Flash] COCOSDC.ROM not found on SD (root or /ROMS/)");
            status_flags |= FLASH_STATUS_SDC_MISSING;
            return false;
        }
    }

    if (!install_rom(SLOT_COCOSDC, rom_buf, rom_size, 0xC000, "SDC-DOS", 1)) {
        status_flags |= FLASH_STATUS_SDC_MISSING;
        return false;
    }

    install_rom(bank_to_slot(0), rom_buf, rom_size, 0xC000, "SDC-DOS", 1);
    status_flags &= ~FLASH_STATUS_SDC_MISSING;
    Serial.println("[Flash] SDC-DOS installed from SD");
    return true;
}

void FlashRomManager::init_factory_defaults(bool force_reinstall) {
    // --- Slot 0: CoPico X-BIOS (embedded via build_bios.py) ---
    if (force_reinstall || is_slot_empty(SLOT_XBIOS)) {
        Serial.println("[Flash] Installing CoPico X-BIOS to Slot 0...");
        install_rom(SLOT_XBIOS,
                    copico_xbios_bin,
                    sizeof(copico_xbios_bin),
                    0xC000,
                    "CoPico X-BIOS",
                    1);
    }

    // --- Slot 1: SDC-DOS — NOT embedded, must come from SD card ---
    // The boot sequence in main.cpp handles this via the ESP32 bridge.
    // We just set the status flag so the Chameleon BIOS can show a message.
    if (is_slot_empty(SLOT_COCOSDC)) {
        status_flags |= FLASH_STATUS_SDC_MISSING;
        Serial.println("[Flash] SDC-DOS not in flash. Waiting for SD card install.");
    } else {
        status_flags &= ~FLASH_STATUS_SDC_MISSING;
    }

    // --- Slot 2: FujiNet BIOS ---
    // Full binary embedded when available; placeholder for now.
    // The ESP32 can download and install the real binary via the Boot Menu.
    if (force_reinstall || is_slot_empty(SLOT_FUJINET)) {
        Serial.println("[Flash] Installing FujiNet placeholder to Slot 2...");
        install_rom(SLOT_FUJINET,
                    FUJINET_PLACEHOLDER,
                    sizeof(FUJINET_PLACEHOLDER),
                    0xC000,
                    "FujiNet (Pending)",
                    0);
    }

    // --- Slot 3: RS-232 Pak ROM (abandonware, same precedent as CoCoSDC) ---
    if (force_reinstall || is_slot_empty(SLOT_RS232)) {
        Serial.println("[Flash] Installing RS-232 Pak ROM to Slot 3...");
        install_rom(SLOT_RS232,
                    rs232_pak_bin,
                    rs232_pak_bin_len,
                    0xC000,
                    "RS-232 Deluxe Pak",
                    1);
    }

    // --- Bank 1 (Slot 5): Disk BASIC 1.1 ---
    // Same ROM pre-loaded on the original CoCoSDC hardware.
    if (force_reinstall || is_slot_empty(BANK_SDC_BASE + 1)) {
        Serial.println("[Flash] Installing Disk BASIC 1.1 to Bank 1...");
        install_rom(BANK_SDC_BASE + 1,
                    disk_basic_11_bin,
                    disk_basic_11_bin_len,
                    0xC000,
                    "Disk BASIC 1.1",
                    1);
    }
}
