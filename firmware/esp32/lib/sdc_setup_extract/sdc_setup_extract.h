#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDC_DOS_ROM_SIZE 16384u
#define SDC_DSK_SECTOR_SIZE 256u
#define SDC_DSK_GRANULE_SECTORS 9u

// Extract SDC-DOS.ROM (16KB) from an official CoCoSDC SETUP.DSK image.
// Accepts raw DSK sector arrays (35-track / 161280 bytes and similar sizes).
// Returns true when a valid SDC-DOS ROM image is written to out_rom.
bool sdc_dos_extract_from_setup_dsk(const uint8_t* dsk_data, size_t dsk_size,
                                    uint8_t* out_rom, size_t out_rom_size,
                                    size_t* out_len);

// Validate a candidate ROM buffer (size + SDC-DOS marker + cart reset vector).
bool sdc_dos_rom_is_valid(const uint8_t* rom_data, size_t rom_size);

#ifdef ARDUINO
class FsFile;
bool sdc_dos_extract_from_setup_file(FsFile& file, uint8_t* out_rom,
                                     size_t out_rom_size, size_t* out_len);
#endif

#ifdef __cplusplus
}
#endif
