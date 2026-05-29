#pragma once

#include <stddef.h>
#include <stdint.h>

#include "spi_protocol.h"

void copico_esp32_init();

bool copico_esp32_transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet);
bool copico_esp32_rom_fetch_file(const char* path, uint8_t* buffer, size_t max_len,
                                 size_t* out_len);
