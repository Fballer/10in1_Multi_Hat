#include "sdc_setup_extract.h"

#include <string.h>

namespace {

const char kSdcDosMarker[] = "SDC-DOS";

const uint8_t* find_bytes(const uint8_t* haystack, size_t haystack_len,
                          const char* needle) {
  const size_t needle_len = strlen(needle);
  if (needle_len == 0 || needle_len > haystack_len) {
    return nullptr;
  }
  for (size_t i = 0; i + needle_len <= haystack_len; ++i) {
    if (memcmp(haystack + i, needle, needle_len) == 0) {
      return haystack + i;
    }
  }
  return nullptr;
}

bool name_equals_ci(const char* a, const char* b) {
  while (*a && *b) {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
    if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
    if (ca != cb) return false;
  }
  return *a == *b;
}

constexpr size_t kGranuleBytes = SDC_DSK_GRANULE_SECTORS * SDC_DSK_SECTOR_SIZE;

bool name_matches(const uint8_t entry[32], const char* name8, const char* ext3) {
  char file_name[9];
  char file_ext[4];
  memcpy(file_name, entry, 8);
  file_name[8] = '\0';
  memcpy(file_ext, entry + 8, 3);
  file_ext[3] = '\0';

  for (char* p = file_name; *p; ++p) {
    if (*p == ' ') *p = '\0';
  }
  for (char* p = file_ext; *p; ++p) {
    if (*p == ' ') *p = '\0';
  }

  return name_equals_ci(file_name, name8) && name_equals_ci(file_ext, ext3);
}

bool read_sector(const uint8_t* dsk, size_t dsk_size, uint32_t lsn,
                 uint8_t sector[SDC_DSK_SECTOR_SIZE]) {
  const size_t offset = (size_t)lsn * SDC_DSK_SECTOR_SIZE;
  if (offset + SDC_DSK_SECTOR_SIZE > dsk_size) {
    return false;
  }
  memcpy(sector, dsk + offset, SDC_DSK_SECTOR_SIZE);
  return true;
}

bool extract_via_decb_entry(const uint8_t* dsk, size_t dsk_size,
                            const uint8_t entry[32], uint8_t* out_rom,
                            size_t out_rom_size, size_t* out_len) {
  const uint8_t granules = entry[25];
  const uint32_t lsn =
      (uint32_t)entry[26] | ((uint32_t)entry[27] << 8) | ((uint32_t)entry[28] << 16);
  const uint32_t last_used =
      (uint32_t)entry[29] | ((uint32_t)entry[30] << 8) | ((uint32_t)entry[31] << 16);

  if (granules == 0 || lsn == 0) {
    return false;
  }

  const size_t total_size =
      (size_t)(granules - 1) * kGranuleBytes + (size_t)last_used;
  if (total_size == 0 || total_size > out_rom_size) {
    return false;
  }

  size_t written = 0;
  for (uint8_t g = 0; g < granules; ++g) {
    const uint32_t granule_lsn = lsn + (uint32_t)g * SDC_DSK_GRANULE_SECTORS;
    for (uint8_t s = 0; s < SDC_DSK_GRANULE_SECTORS; ++s) {
      uint8_t sector[SDC_DSK_SECTOR_SIZE];
      if (!read_sector(dsk, dsk_size, granule_lsn + s, sector)) {
        return false;
      }

      size_t chunk = SDC_DSK_SECTOR_SIZE;
      if (written + chunk > total_size) {
        chunk = total_size - written;
      }
      memcpy(out_rom + written, sector, chunk);
      written += chunk;
      if (written >= total_size) {
        break;
      }
    }
    if (written >= total_size) {
      break;
    }
  }

  if (!sdc_dos_rom_is_valid(out_rom, written)) {
    return false;
  }

  *out_len = written;
  return true;
}

bool find_directory_entry(const uint8_t* dsk, size_t dsk_size,
                          const char* name8, const char* ext3,
                          uint8_t entry_out[32]) {
  if (dsk_size < SDC_DSK_SECTOR_SIZE) {
    return false;
  }

  const size_t sector_count = dsk_size / SDC_DSK_SECTOR_SIZE;
  for (size_t sector = 0; sector < sector_count; ++sector) {
    const uint8_t* dir = dsk + sector * SDC_DSK_SECTOR_SIZE;
    for (size_t i = 0; i < 8; ++i) {
      const uint8_t* entry = dir + i * 32;
      if (entry[0] == 0 || entry[0] == 0xFF) {
        continue;
      }
      if (name_matches(entry, name8, ext3)) {
        memcpy(entry_out, entry, 32);
        return true;
      }
    }
  }
  return false;
}

bool extract_via_signature_scan(const uint8_t* dsk, size_t dsk_size,
                                uint8_t* out_rom, size_t out_rom_size,
                                size_t* out_len) {
  if (out_rom_size < SDC_DOS_ROM_SIZE || dsk_size < SDC_DOS_ROM_SIZE) {
    return false;
  }

  size_t best_offset = dsk_size;
  bool found = false;

  for (size_t offset = 0; offset + SDC_DOS_ROM_SIZE <= dsk_size;
       offset += SDC_DSK_SECTOR_SIZE) {
    const uint8_t* candidate = dsk + offset;
    if (!sdc_dos_rom_is_valid(candidate, SDC_DOS_ROM_SIZE)) {
      continue;
    }
    if (offset < best_offset) {
      best_offset = offset;
      found = true;
    }
  }

  if (!found) {
    return false;
  }

  memcpy(out_rom, dsk + best_offset, SDC_DOS_ROM_SIZE);
  *out_len = SDC_DOS_ROM_SIZE;
  return true;
}

}  // namespace

bool sdc_dos_rom_is_valid(const uint8_t* rom_data, size_t rom_size) {
  if (!rom_data || rom_size < SDC_DOS_ROM_SIZE) {
    return false;
  }

  if (find_bytes(rom_data, rom_size, kSdcDosMarker) == nullptr) {
    return false;
  }

  const uint16_t reset_vector =
      ((uint16_t)rom_data[0x3FFE] << 8) | (uint16_t)rom_data[0x3FFF];
  return reset_vector >= 0xC000 && reset_vector <= 0xFFFE;
}

bool sdc_dos_extract_from_setup_dsk(const uint8_t* dsk_data, size_t dsk_size,
                                    uint8_t* out_rom, size_t out_rom_size,
                                    size_t* out_len) {
  if (!dsk_data || !out_rom || !out_len || out_rom_size < SDC_DOS_ROM_SIZE) {
    return false;
  }

  uint8_t entry[32];
  if (find_directory_entry(dsk_data, dsk_size, "SDC-DOS", "ROM", entry)) {
    if (extract_via_decb_entry(dsk_data, dsk_size, entry, out_rom, out_rom_size,
                               out_len)) {
      return true;
    }
  }

  return extract_via_signature_scan(dsk_data, dsk_size, out_rom, out_rom_size,
                                    out_len);
}

#ifdef ARDUINO
#include <SdFat.h>

bool sdc_dos_extract_from_setup_file(FsFile& file, uint8_t* out_rom,
                                     size_t out_rom_size, size_t* out_len) {
  if (!out_rom || !out_len || out_rom_size < SDC_DOS_ROM_SIZE) {
    return false;
  }

  const size_t dsk_size = file.size();
  if (dsk_size < SDC_DOS_ROM_SIZE) {
    return false;
  }

  uint8_t sector[SDC_DSK_SECTOR_SIZE];
  uint8_t entry[32];
  bool found_entry = false;

  const size_t sector_count = dsk_size / SDC_DSK_SECTOR_SIZE;
  for (size_t sec = 0; sec < sector_count; ++sec) {
    file.seek(sec * SDC_DSK_SECTOR_SIZE);
    if (file.read(sector, sizeof(sector)) != (int)sizeof(sector)) {
      continue;
    }
    for (size_t i = 0; i < 8; ++i) {
      const uint8_t* ent = sector + i * 32;
      if (ent[0] == 0 || ent[0] == 0xFF) {
        continue;
      }
      if (name_matches(ent, "SDC-DOS", "ROM")) {
        memcpy(entry, ent, sizeof(entry));
        found_entry = true;
        break;
      }
    }
    if (found_entry) {
      break;
    }
  }

  if (found_entry) {
    const uint8_t granules = entry[25];
    const uint32_t lsn =
        (uint32_t)entry[26] | ((uint32_t)entry[27] << 8) | ((uint32_t)entry[28] << 16);
    const uint32_t last_used =
        (uint32_t)entry[29] | ((uint32_t)entry[30] << 8) | ((uint32_t)entry[31] << 16);
    if (granules > 0 && lsn > 0) {
      const size_t total_size =
          (size_t)(granules - 1) * kGranuleBytes + (size_t)last_used;
      if (total_size > 0 && total_size <= out_rom_size) {
        size_t written = 0;
        bool ok = true;
        for (uint8_t g = 0; g < granules && ok; ++g) {
          const uint32_t granule_lsn = lsn + (uint32_t)g * SDC_DSK_GRANULE_SECTORS;
          for (uint8_t s = 0; s < SDC_DSK_GRANULE_SECTORS; ++s) {
            const size_t offset = (size_t)(granule_lsn + s) * SDC_DSK_SECTOR_SIZE;
            if (offset + SDC_DSK_SECTOR_SIZE > dsk_size) {
              ok = false;
              break;
            }
            file.seek(offset);
            if (file.read(sector, sizeof(sector)) != (int)sizeof(sector)) {
              ok = false;
              break;
            }
            size_t chunk = SDC_DSK_SECTOR_SIZE;
            if (written + chunk > total_size) {
              chunk = total_size - written;
            }
            memcpy(out_rom + written, sector, chunk);
            written += chunk;
            if (written >= total_size) {
              break;
            }
          }
        }
        if (ok && sdc_dos_rom_is_valid(out_rom, written)) {
          *out_len = written;
          return true;
        }
      }
    }
  }

  for (size_t offset = 0; offset + SDC_DOS_ROM_SIZE <= dsk_size;
       offset += SDC_DSK_SECTOR_SIZE) {
    file.seek(offset);
    if (file.read(out_rom, SDC_DOS_ROM_SIZE) != (int)SDC_DOS_ROM_SIZE) {
      continue;
    }
    if (sdc_dos_rom_is_valid(out_rom, SDC_DOS_ROM_SIZE)) {
      *out_len = SDC_DOS_ROM_SIZE;
      return true;
    }
  }

  return false;
}
#endif
