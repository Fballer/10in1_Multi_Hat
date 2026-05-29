#include "copico_esp32_bridge.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint BRIDGE_CS = 23;
constexpr uint BRIDGE_MISO = 24;
constexpr uint BRIDGE_SCK = 30;
constexpr uint BRIDGE_MOSI = 31;

constexpr uint32_t kSpiTimeoutSdMs = 500;
constexpr uint32_t kSpiTimeoutCmdMs = 100;
constexpr uint8_t kSpiMaxRetries = 3;

spi_inst_t* kSpi = spi0;

void transfer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
  gpio_put(BRIDGE_CS, 0);
  sleep_us(5);
  spi_write_read_blocking(kSpi, tx_buf, rx_buf, len);
  gpio_put(BRIDGE_CS, 1);
  sleep_us(50);
}

uint32_t timeout_for_command(uint8_t cmd) {
  switch (cmd) {
    case CMD_SDC_READ:
    case CMD_SDC_WRITE:
    case CMD_SDC_MOUNT:
    case CMD_ROM_FETCH:
    case CMD_ROM_READ_CHUNK:
      return kSpiTimeoutSdMs;
    default:
      return kSpiTimeoutCmdMs;
  }
}

}  // namespace

void copico_esp32_init() {
  gpio_init(BRIDGE_CS);
  gpio_set_dir(BRIDGE_CS, GPIO_OUT);
  gpio_put(BRIDGE_CS, 1);

  gpio_set_function(BRIDGE_MISO, GPIO_FUNC_SPI);
  gpio_set_function(BRIDGE_SCK, GPIO_FUNC_SPI);
  gpio_set_function(BRIDGE_MOSI, GPIO_FUNC_SPI);

  spi_init(kSpi, 1 * 1000 * 1000);
  spi_set_format(kSpi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

bool copico_esp32_transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet) {
  if (!tx_packet || !rx_packet) {
    return false;
  }

  const uint32_t timeout_ms = timeout_for_command(tx_packet->command);

  tx_packet->sync = SPI_SYNC_BYTE;
  tx_packet->checksum =
      calc_checksum(reinterpret_cast<uint8_t*>(tx_packet), sizeof(SpiMasterPacket) - 1);
  transfer(reinterpret_cast<uint8_t*>(tx_packet), reinterpret_cast<uint8_t*>(rx_packet),
           sizeof(SpiMasterPacket));

  if (rx_packet->sync == SPI_SYNC_BYTE &&
      calc_checksum(reinterpret_cast<uint8_t*>(rx_packet), sizeof(SpiSlavePacket) - 1) ==
          rx_packet->checksum &&
      rx_packet->status != STATUS_SDC_BUSY) {
    return true;
  }

  SpiMasterPacket poll_packet{};
  poll_packet.sync = SPI_SYNC_BYTE;
  poll_packet.command = CMD_POLL;
  poll_packet.checksum =
      calc_checksum(reinterpret_cast<uint8_t*>(&poll_packet), sizeof(poll_packet) - 1);

  const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  uint8_t retries = 0;
  uint8_t bad_checksums = 0;

  while (!time_reached(deadline)) {
    sleep_ms(1);
    transfer(reinterpret_cast<uint8_t*>(&poll_packet),
             reinterpret_cast<uint8_t*>(rx_packet), sizeof(SpiMasterPacket));

    if (rx_packet->sync != SPI_SYNC_BYTE) {
      if (++retries >= kSpiMaxRetries) {
        break;
      }
      continue;
    }

    const uint8_t expected =
        calc_checksum(reinterpret_cast<uint8_t*>(rx_packet), sizeof(SpiSlavePacket) - 1);
    if (expected != rx_packet->checksum) {
      if (++bad_checksums >= kSpiMaxRetries) {
        printf("[SPI] checksum errors on cmd 0x%02x\n", tx_packet->command);
        break;
      }
      continue;
    }

    if (rx_packet->status != STATUS_SDC_BUSY) {
      return true;
    }
    retries = 0;
  }

  printf("[SPI] timeout cmd=0x%02x after %u ms\n", tx_packet->command, timeout_ms);
  return false;
}

bool copico_esp32_rom_fetch_file(const char* path, uint8_t* buffer, size_t max_len,
                                 size_t* out_len) {
  if (!path || !buffer || !out_len) {
    return false;
  }

  SpiMasterPacket tx{};
  SpiSlavePacket rx{};

  tx.command = CMD_ROM_FETCH;
  size_t path_len = strlen(path);
  if (path_len >= SPI_PAYLOAD_SIZE) {
    path_len = SPI_PAYLOAD_SIZE - 1;
  }
  memcpy(tx.payload, path, path_len);
  tx.payload[path_len] = '\0';
  tx.length = static_cast<uint8_t>(path_len + 1);

  if (!copico_esp32_transaction(&tx, &rx)) {
    return false;
  }
  if (rx.status != STATUS_ROM_ACK || rx.payload[0] != 0) {
    return false;
  }

  const uint16_t total =
      static_cast<uint16_t>(rx.payload[1]) | (static_cast<uint16_t>(rx.payload[2]) << 8);
  if (total == 0 || total > max_len) {
    return false;
  }

  size_t offset = 0;
  while (offset < total) {
    memset(&tx, 0, sizeof(tx));
    tx.command = CMD_ROM_READ_CHUNK;
    tx.payload[0] = static_cast<uint8_t>(offset & 0xFF);
    tx.payload[1] = static_cast<uint8_t>((offset >> 8) & 0xFF);
    tx.length = 2;

    if (!copico_esp32_transaction(&tx, &rx)) {
      return false;
    }
    if (rx.status != STATUS_ROM_CHUNK) {
      return false;
    }

    const uint8_t chunk_len = rx.payload[0];
    if (chunk_len == 0 || offset + chunk_len > total) {
      return false;
    }
    memcpy(buffer + offset, &rx.payload[1], chunk_len);
    offset += chunk_len;
  }

  *out_len = total;
  return true;
}
