#pragma once
#include <stdint.h>

// Fixed packet size for Master Polling over SPI
#define SPI_PACKET_SIZE 264
#define SPI_PAYLOAD_SIZE (SPI_PACKET_SIZE - 4)

// Sync byte to ensure packet alignment
#define SPI_SYNC_BYTE 0xAA

// Master (RP2350) to Slave (ESP32) Commands
#define CMD_POLL 0x01
#define CMD_TX_DATA 0x02
#define CMD_RESET 0x03
#define CMD_SDC_READ 0x10   // Payload: [0]=Drive, [1..3]=LSN
#define CMD_SDC_WRITE 0x11  // Payload: [0]=Drive, [1..3]=LSN, [4..259]=Data
#define CMD_SDC_MOUNT 0x12  // Payload: [0]=Drive, [1..n]=Null-terminated filename
#define CMD_SDC_SWAP  0x13  // Payload: Empty. Tells ESP32 to mount next disk.
#define CMD_SET_TIMEZONE 0x21 // Payload: [0]=Timezone Index
#define CMD_ROM_FETCH      0x14 // Payload: null-terminated path on SD
#define CMD_ROM_READ_CHUNK 0x15 // Payload: [0-1]=byte offset (little-endian)

// Slave (ESP32) to Master (RP2350) Status
#define STATUS_IDLE 0x01
#define STATUS_HAS_DATA 0x02
#define STATUS_SDC_SECTOR 0x10 // Payload: [0..255]=Sector Data
#define STATUS_SDC_ACK 0x11    // Payload: [0]=Error Code (0=Success)
#define STATUS_SDC_BUSY 0x12   // Payload: Empty. ESP32 is reading/writing.
#define STATUS_SYS_INFO 0x20   // Payload: SpiSysInfoPayload
#define STATUS_ROM_ACK    0x22 // Payload: [0]=err (0=ok), [1-2]=size LE, [3]=max chunk
#define STATUS_ROM_CHUNK  0x23 // Payload: [0]=len, [1..]=data

// Packet structure (ensure 1-byte alignment)
#pragma pack(push, 1)

// Master -> Slave Packet
struct SpiMasterPacket {
    uint8_t sync;       // Always SPI_SYNC_BYTE
    uint8_t command;    // CMD_POLL, CMD_TX_DATA, etc.
    uint8_t length;     // Number of valid bytes in payload
    uint8_t payload[SPI_PAYLOAD_SIZE];
    uint8_t checksum;   // Simple XOR checksum of everything before it
};

// Slave -> Master Packet
struct SpiSlavePacket {
    uint8_t sync;       // Always SPI_SYNC_BYTE
    uint8_t status;     // STATUS_IDLE, STATUS_HAS_DATA
    uint8_t length;     // Number of valid bytes in payload
    uint8_t payload[SPI_PAYLOAD_SIZE];
    uint8_t checksum;   // Simple XOR checksum of everything before it
};

// Payload definition for STATUS_SYS_INFO
struct SpiSysInfoPayload {
    char wifi_status[32]; // $C800
    char fw_version[32];  // $C820
    char sd_status[32];   // $C840
    char tz_name[32];     // $C860
    uint32_t ntp_timestamp;
};

#pragma pack(pop)

// Helper to calculate checksum
inline uint8_t calc_checksum(const uint8_t* data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}
