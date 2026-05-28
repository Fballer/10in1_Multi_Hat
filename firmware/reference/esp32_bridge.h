#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "spi_protocol.h"

class Esp32Bridge {
public:
    Esp32Bridge();
    void init();
    
    // Core transaction (blocks with tiered timeout + retry limit — see esp32_bridge.cpp)
    bool transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet);

    // Fire-and-forget command (wraps transaction, ignores response)
    bool send_command(uint8_t cmd, const uint8_t* payload, uint8_t len);

    // SDC Specific Helpers
    bool sdc_read_sector(uint8_t drive_id, uint32_t lsn, uint8_t* buffer);
    bool sdc_write_sector(uint8_t drive_id, uint32_t lsn, const uint8_t* buffer);
    bool sdc_mount(uint8_t drive_id, const char* filename);
    bool sdc_swap();

    // Read a ROM file from the ESP32 SD card into buffer (max 16KB).
    bool rom_fetch_file(const char* path, uint8_t* buffer, size_t max_len, size_t* out_len);

private:
    void send_packet(SpiMasterPacket* packet);
    bool receive_packet(SpiSlavePacket* packet);
    void transfer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len);
};

extern Esp32Bridge esp32;
