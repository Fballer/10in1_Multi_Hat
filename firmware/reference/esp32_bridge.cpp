#include "esp32_bridge.h"

extern volatile bool spi_bus_locked;

// ============================================================
// SPI Bridge Pin Definitions — Verified against PCB Netlist
// (Centipede 32z Hat-Fuji-40C, schematic dated 2026-04-19)
// J4 Header Mapping:
//   Pin 7  -> ESP_C3_CS_G23   -> ESP32 GPIO4
//   Pin 9  -> ESP_C3_MISO_G24 -> ESP32 GPIO0
//   Pin 15 -> ESP_C3_SCK_G30  -> ESP32 GPIO3
//   Pin 16 -> ESP_C3_MOSI_G31 -> ESP32 GPIO1
// ============================================================
#define BRIDGE_CS   23
#define BRIDGE_MISO 24
#define BRIDGE_SCK  30
#define BRIDGE_MOSI 31

Esp32Bridge esp32;

Esp32Bridge::Esp32Bridge() {
}

void Esp32Bridge::init() {
    pinMode(BRIDGE_CS, OUTPUT);
    digitalWrite(BRIDGE_CS, HIGH);

    // Initialize SPI hardware on RP2350
    SPI.setRX(BRIDGE_MISO);
    SPI.setSCK(BRIDGE_SCK);
    SPI.setTX(BRIDGE_MOSI);
    SPI.begin();
}

void Esp32Bridge::transfer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
    digitalWrite(BRIDGE_CS, LOW);
    // Give the ESP32 SPI Slave a tiny microsecond delay to recognize CS
    delayMicroseconds(5); 
    
    for (size_t i = 0; i < len; i++) {
        rx_buf[i] = SPI.transfer(tx_buf[i]);
    }
    
    digitalWrite(BRIDGE_CS, HIGH);
    // Give ESP32 time to process after transaction
    delayMicroseconds(50); 
}

// ================================================================
// SPI Transaction with Deadlock Insurance
// ================================================================
// WHY: If the ESP32 hangs (brownout, firmware crash, SD card fault),
//      the old code would spin for 5 seconds in a while() loop.
//      During that time, Core 0 is frozen — which means cocosdc.tick()
//      blocks, the WiModem stream stops, and the status LED dies.
//
// FIX: Tiered timeouts + retry counter + diagnostic output.
//   - SD operations: 500ms max (worst-case FAT32 seek)
//   - Simple commands: 100ms max
//   - Max 3 retries before giving up
//   - Serial diagnostic on every failure for debug
// ================================================================

#define SPI_TIMEOUT_SD_MS     500   // Max wait for SD read/write
#define SPI_TIMEOUT_CMD_MS    100   // Max wait for simple commands
#define SPI_MAX_RETRIES       3     // Retry count before hard-fail

bool Esp32Bridge::transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet) {
    spi_bus_locked = true;

    // Choose timeout based on command type
    uint32_t timeout_ms;
    switch (tx_packet->command) {
        case CMD_SDC_READ:
        case CMD_SDC_WRITE:
        case CMD_SDC_MOUNT:
        case CMD_ROM_FETCH:
        case CMD_ROM_READ_CHUNK:
            timeout_ms = SPI_TIMEOUT_SD_MS;
            break;
        default:
            timeout_ms = SPI_TIMEOUT_CMD_MS;
            break;
    }

    // 1. Send the primary packet
    tx_packet->sync = SPI_SYNC_BYTE;
    tx_packet->checksum = calc_checksum((uint8_t*)tx_packet, sizeof(SpiMasterPacket) - 1);

    transfer((uint8_t*)tx_packet, (uint8_t*)rx_packet, sizeof(SpiMasterPacket));

    // 2. Check if first response is already valid and terminal
    if (rx_packet->sync == SPI_SYNC_BYTE &&
        calc_checksum((uint8_t*)rx_packet, sizeof(SpiSlavePacket) - 1) == rx_packet->checksum &&
        rx_packet->status != STATUS_SDC_BUSY) {
        spi_bus_locked = false;
        return true;
    }

    // 3. Poll with timeout and retry limit
    SpiMasterPacket poll_packet;
    memset(&poll_packet, 0, sizeof(poll_packet));
    poll_packet.sync = SPI_SYNC_BYTE;
    poll_packet.command = CMD_POLL;
    poll_packet.checksum = calc_checksum((uint8_t*)&poll_packet, sizeof(poll_packet) - 1);

    uint32_t deadline = millis() + timeout_ms;
    uint8_t retries = 0;
    uint8_t bad_checksums = 0;

    while (millis() < deadline) {
        delay(1);
        transfer((uint8_t*)&poll_packet, (uint8_t*)rx_packet, sizeof(SpiMasterPacket));

        // Validate response
        if (rx_packet->sync != SPI_SYNC_BYTE) {
            retries++;
            if (retries >= SPI_MAX_RETRIES) break;
            continue;
        }

        uint8_t expected = calc_checksum((uint8_t*)rx_packet, sizeof(SpiSlavePacket) - 1);
        if (expected != rx_packet->checksum) {
            bad_checksums++;
            if (bad_checksums >= SPI_MAX_RETRIES) {
                Serial.println("[SPI] FAIL: repeated checksum errors");
                break;
            }
            continue;
        }

        if (rx_packet->status != STATUS_SDC_BUSY) {
            spi_bus_locked = false;
            return true; // Success
        }
        // Still busy — keep polling (reset retry counter since ESP32 is alive)
        retries = 0;
    }

    // Timeout or hard failure
    Serial.print("[SPI] FAIL: cmd=0x");
    Serial.print(tx_packet->command, HEX);
    Serial.print(" timeout=");
    Serial.print(timeout_ms);
    Serial.print("ms retries=");
    Serial.print(retries);
    Serial.print(" bad_csum=");
    Serial.println(bad_checksums);

    spi_bus_locked = false;
    return false;
}

bool Esp32Bridge::send_command(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));

    tx.command = cmd;
    tx.length = len;
    if (payload && len > 0 && len <= SPI_PAYLOAD_SIZE) {
        memcpy(tx.payload, payload, len);
    }

    return transaction(&tx, &rx);
}

bool Esp32Bridge::sdc_read_sector(uint8_t drive_id, uint32_t lsn, uint8_t* buffer) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_READ;
    tx.length = 4;
    tx.payload[0] = drive_id;
    tx.payload[1] = (lsn >> 16) & 0xFF;
    tx.payload[2] = (lsn >> 8) & 0xFF;
    tx.payload[3] = lsn & 0xFF;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_SECTOR) {
            memcpy(buffer, rx.payload, 256);
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_write_sector(uint8_t drive_id, uint32_t lsn, const uint8_t* buffer) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_WRITE;
    tx.length = 0; // Length is implied 260 bytes for SDC_WRITE
    tx.payload[0] = drive_id;
    tx.payload[1] = (lsn >> 16) & 0xFF;
    tx.payload[2] = (lsn >> 8) & 0xFF;
    tx.payload[3] = lsn & 0xFF;
    memcpy(&tx.payload[4], buffer, 256);
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_mount(uint8_t drive_id, const char* filename) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_MOUNT;
    tx.payload[0] = drive_id;
    
    size_t len = strlen(filename);
    if (len > 250) len = 250; // Safety limit
    
    memcpy(&tx.payload[1], filename, len);
    tx.payload[1 + len] = '\0';
    tx.length = 1 + len + 1;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_swap() {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_SWAP;
    tx.length = 0;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::rom_fetch_file(const char* path, uint8_t* buffer, size_t max_len, size_t* out_len) {
    if (!path || !buffer || !out_len) return false;

    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));

    tx.command = CMD_ROM_FETCH;
    size_t path_len = strlen(path);
    if (path_len >= SPI_PAYLOAD_SIZE) path_len = SPI_PAYLOAD_SIZE - 1;
    memcpy(tx.payload, path, path_len);
    tx.payload[path_len] = '\0';
    tx.length = (uint8_t)(path_len + 1);

    if (!transaction(&tx, &rx)) return false;
    if (rx.status != STATUS_ROM_ACK || rx.payload[0] != 0) return false;

    uint16_t total = (uint16_t)rx.payload[1] | ((uint16_t)rx.payload[2] << 8);
    if (total == 0 || total > max_len) return false;

    size_t offset = 0;
    while (offset < total) {
        memset(&tx, 0, sizeof(tx));
        tx.command = CMD_ROM_READ_CHUNK;
        tx.payload[0] = (uint8_t)(offset & 0xFF);
        tx.payload[1] = (uint8_t)((offset >> 8) & 0xFF);
        tx.length = 2;

        if (!transaction(&tx, &rx)) return false;
        if (rx.status != STATUS_ROM_CHUNK) return false;

        uint8_t chunk_len = rx.payload[0];
        if (chunk_len == 0 || offset + chunk_len > total) return false;
        memcpy(buffer + offset, &rx.payload[1], chunk_len);
        offset += chunk_len;
    }

    *out_len = total;
    return true;
}
