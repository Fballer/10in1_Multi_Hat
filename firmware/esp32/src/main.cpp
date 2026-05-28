#include <Arduino.h>
#include <driver/spi_slave.h>
#include <driver/gpio.h>
#include "spi_protocol.h"
#include "wimodem.h"
#include "SdFat.h"
#include "sdios.h"
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_SOFTWARE_SPI_CLASS
SoftSpiDriver<SD_MISO, SD_MOSI, SD_SCK> softSpi;
#endif

// Define ESP32-C3 pins connected to RP2350 SPI (Hardware Layout Phase 10)
#define ESP_SPI_MISO 0
#define ESP_SPI_MOSI 1
#define ESP_SPI_SCK  3
#define ESP_SPI_CS   4

#define SPI_HOST SPI2_HOST // FSPI on ESP32-C3

// Define SD Card Pins (Software SPI because SPI2 is used by Slave)
#define SD_MOSI 5
#define SD_MISO 6
#define SD_SCK  7
#define SD_CS   10

SoftSpiDriver<SD_MISO, SD_MOSI, SD_SCK> softSpi;
SdFat sd;
File drives[2];

// Buffers for SPI DMA (Must be DMA-capable memory)
WORD_ALIGNED_ATTR SpiMasterPacket rx_packet;
WORD_ALIGNED_ATTR SpiSlavePacket tx_packet;

// Internal FIFOs to act as our Stream buffers for the ModemEngine
#define ESP_FIFO_SIZE 1024
uint8_t modem_rx_fifo[ESP_FIFO_SIZE]; // Data coming FROM RP2350
volatile uint16_t modem_rx_head = 0;
volatile uint16_t modem_rx_tail = 0;

uint8_t modem_tx_fifo[ESP_FIFO_SIZE]; // Data going TO RP2350
volatile uint16_t modem_tx_head = 0;
volatile uint16_t modem_tx_tail = 0;

void spi_slave_init() {
    // Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = ESP_SPI_MOSI,
        .miso_io_num = ESP_SPI_MISO,
        .sclk_io_num = ESP_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };

    // Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = ESP_SPI_CS,
        .flags = 0,
        .queue_size = 3,
        .mode = 0,
        .post_setup_cb = NULL,
        .post_trans_cb = NULL
    };

    // Initialize SPI slave interface
    esp_err_t ret = spi_slave_initialize(SPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    assert(ret == ESP_OK);
}

void process_spi_transaction() {
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    // We assume the RP2350 will always send exactly sizeof(SpiMasterPacket)
    t.length = sizeof(SpiMasterPacket) * 8; 
    t.tx_buffer = &tx_packet;
    t.rx_buffer = &rx_packet;

    // Wait for the master to initiate the transaction
    esp_err_t ret = spi_slave_transmit(SPI_HOST, &t, portMAX_DELAY);
    if (ret == ESP_OK) {
        // Validate RX Packet from RP2350
        if (rx_packet.sync == SPI_SYNC_BYTE) {
            uint8_t expected_checksum = calc_checksum((const uint8_t*)&rx_packet, sizeof(rx_packet) - 1);
            if (expected_checksum == rx_packet.checksum) {
                
                // --- HANDLE INCOMING COMMANDS ---
                if (rx_packet.command == CMD_TX_DATA && rx_packet.length <= SPI_PAYLOAD_SIZE) {
                    for (int i = 0; i < rx_packet.length; i++) {
                        uint16_t next_head = (modem_rx_head + 1) % ESP_FIFO_SIZE;
                        if (next_head != modem_rx_tail) {
                            modem_rx_fifo[modem_rx_head] = rx_packet.payload[i];
                            modem_rx_head = next_head;
                        }
                    }
                } 
                else if (rx_packet.command == CMD_SDC_READ) {
                    uint8_t drive_id = rx_packet.payload[0];
                    uint32_t lsn = ((uint32_t)rx_packet.payload[1] << 16) | 
                                   ((uint32_t)rx_packet.payload[2] << 8) | 
                                   rx_packet.payload[3];
                                   
                    if (drive_id < 2 && drives[drive_id]) {
                        drives[drive_id].seekSet(lsn * 256);
                        int bytes_read = drives[drive_id].read(tx_packet.payload, 256);
                        if (bytes_read < 256) memset(tx_packet.payload + bytes_read, 0, 256 - bytes_read);
                        
                        tx_packet.status = STATUS_SDC_SECTOR;
                        tx_packet.length = 0; // length ignored for SDC_SECTOR
                    } else {
                        tx_packet.status = STATUS_SDC_ACK; // Will send error code
                        tx_packet.payload[0] = 0x80; // Error
                        tx_packet.length = 1;
                    }
                }
                else if (rx_packet.command == CMD_SDC_WRITE) {
                    uint8_t drive_id = rx_packet.payload[0];
                    uint32_t lsn = ((uint32_t)rx_packet.payload[1] << 16) | 
                                   ((uint32_t)rx_packet.payload[2] << 8) | 
                                   rx_packet.payload[3];
                                   
                    if (drive_id < 2 && drives[drive_id]) {
                        drives[drive_id].seekSet(lsn * 256);
                        drives[drive_id].write(&rx_packet.payload[4], 256);
                        drives[drive_id].sync();
                        
                        tx_packet.status = STATUS_SDC_ACK;
                        tx_packet.payload[0] = 0x00; // Success
                        tx_packet.length = 1;
                    } else {
                        tx_packet.status = STATUS_SDC_ACK;
                        tx_packet.payload[0] = 0x80; // Error
                        tx_packet.length = 1;
                    }
                }
                else if (rx_packet.command == CMD_SDC_MOUNT) {
                    uint8_t drive_id = rx_packet.payload[0];
                    char filename[250];
                    memcpy(filename, &rx_packet.payload[1], rx_packet.length - 1);
                    filename[rx_packet.length - 1] = '\0';
                    
                    if (drive_id < 2) {
                        if (drives[drive_id]) drives[drive_id].close();
                        drives[drive_id] = sd.open(filename, O_RDWR | O_CREAT);
                        
                        tx_packet.status = STATUS_SDC_ACK;
                        tx_packet.payload[0] = drives[drive_id] ? 0x00 : 0x80;
                        tx_packet.length = 1;
                        
                        if (drives[drive_id]) {
                            Serial.printf("ESP32: Mounted '%s' to drive %d\n", filename, drive_id);
                        } else {
                            Serial.printf("ESP32: Failed to mount '%s'\n", filename);
                        }
                    }
                }
                else if (rx_packet.command == CMD_SDC_SWAP) {
                    // TODO: Implement actual disk cycling logic
                    // For now, we will just send an ACK
                    Serial.println("ESP32: Disk Swap Requested");
                    
                    tx_packet.status = STATUS_SDC_ACK;
                    tx_packet.payload[0] = 0x00; // Success
                    tx_packet.length = 1;
                }
            }
        }

        // --- PREPARE THE NEXT TX PACKET ---
        // If we didn't just fulfill an SDC read/write, default to polling WiModem data
        if (rx_packet.command != CMD_SDC_READ && rx_packet.command != CMD_SDC_WRITE && rx_packet.command != CMD_SDC_MOUNT && rx_packet.command != CMD_SDC_SWAP) {
            memset(&tx_packet, 0, sizeof(tx_packet));
            tx_packet.sync = SPI_SYNC_BYTE;
            tx_packet.status = STATUS_IDLE;
            
            uint8_t tx_len = 0;
            while (tx_len < SPI_PAYLOAD_SIZE && modem_tx_head != modem_tx_tail) {
                tx_packet.payload[tx_len++] = modem_tx_fifo[modem_tx_tail];
                modem_tx_tail = (modem_tx_tail + 1) % ESP_FIFO_SIZE;
            }
            
            tx_packet.length = tx_len;
            if (tx_len > 0) {
                tx_packet.status = STATUS_HAS_DATA;
            }
        } else {
            // SDC commands already populated tx_packet.status and tx_packet.payload
            tx_packet.sync = SPI_SYNC_BYTE;
        }
        
        tx_packet.checksum = calc_checksum((const uint8_t*)&tx_packet, sizeof(tx_packet) - 1);
    }
}

// Background task to continuously process SPI transactions
void spi_task(void *pvParameters) {
    // Initial pre-load of a dummy TX packet
    tx_packet.sync = SPI_SYNC_BYTE;
    tx_packet.status = STATUS_IDLE;
    tx_packet.length = 0;
    tx_packet.checksum = calc_checksum((const uint8_t*)&tx_packet, sizeof(tx_packet) - 1);

    while (1) {
        process_spi_transaction();
    }
}

// Forward declarations for WiModem Engine
extern void wimodem_setup();
extern void wimodem_loop();

void setup() {
    Serial.begin(115200); // Debug serial via USB/UART0
    Serial.println("ESP32-C3 Coprocessor Starting...");
    
    spi_slave_init();
    
    // Start SPI processing task on core 0 (ESP32-C3 is single core anyway)
    xTaskCreate(spi_task, "spi_task", 4096, NULL, 5, NULL);
    
    // Initialize SD Card via Software SPI
    if (sd.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(10), &softSpi))) {
        Serial.println("ESP32: SD Card initialized successfully via SoftSPI.");
        
        // Load startup config manually if needed, or wait for RP2350 to request mounts
    } else {
        Serial.println("ESP32: SD Card initialization failed.");
    }
    
    // Initialize WiModem Engine
    wimodem_setup();
}

void loop() {
    // Tick the WiModem Engine
    wimodem_loop();
    delay(1);
}
