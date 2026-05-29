#!/usr/bin/env python3
"""Extract SDC-DOS.ROM from an official CoCoSDC SETUP.DSK image."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROM_SIZE = 16384
SECTOR_SIZE = 256
GRANULE_SECTORS = 9
GRANULE_BYTES = GRANULE_SECTORS * SECTOR_SIZE


def _name_matches(entry: bytes, name8: str, ext3: str) -> bool:
    file_name = entry[0:8].split(b"\x00")[0].decode("ascii", "replace").rstrip()
    file_ext = entry[8:11].split(b"\x00")[0].decode("ascii", "replace").rstrip()
    return file_name.upper() == name8.upper() and file_ext.upper() == ext3.upper()


def rom_is_valid(data: bytes) -> bool:
    if len(data) < ROM_SIZE:
        return False
    if b"SDC-DOS" not in data:
        return False
    reset_vector = (data[0x3FFE] << 8) | data[0x3FFF]
    return 0xC000 <= reset_vector <= 0xFFFE


def extract_via_decb_entry(dsk: bytes, entry: bytes) -> bytes | None:
    granules = entry[25]
    lsn = entry[26] | (entry[27] << 8) | (entry[28] << 16)
    last_used = entry[29] | (entry[30] << 8) | (entry[31] << 16)
    if granules == 0 or lsn == 0:
        return None

    total_size = (granules - 1) * GRANULE_BYTES + last_used
    if total_size == 0:
        return None

    out = bytearray()
    for g in range(granules):
        granule_lsn = lsn + g * GRANULE_SECTORS
        for s in range(GRANULE_SECTORS):
            offset = (granule_lsn + s) * SECTOR_SIZE
            out.extend(dsk[offset : offset + SECTOR_SIZE])
            if len(out) >= total_size:
                break
        if len(out) >= total_size:
            break

    rom = bytes(out[:total_size])
    return rom if rom_is_valid(rom) else None


def find_directory_entry(dsk: bytes, name8: str, ext3: str) -> bytes | None:
    sector_count = len(dsk) // SECTOR_SIZE
    for sector in range(sector_count):
        chunk = dsk[sector * SECTOR_SIZE : (sector + 1) * SECTOR_SIZE]
        for i in range(8):
            entry = chunk[i * 32 : (i + 1) * 32]
            if entry[0] in (0, 0xFF):
                continue
            if _name_matches(entry, name8, ext3):
                return entry
    return None


def extract_via_signature_scan(dsk: bytes) -> bytes | None:
    best = None
    for offset in range(0, len(dsk) - ROM_SIZE + 1, SECTOR_SIZE):
        candidate = dsk[offset : offset + ROM_SIZE]
        if not rom_is_valid(candidate):
            continue
        if best is None or offset < best[0]:
            best = (offset, candidate)
    return best[1] if best else None


def extract_sdc_dos_rom(dsk: bytes) -> bytes:
    entry = find_directory_entry(dsk, "SDC-DOS", "ROM")
    if entry is not None:
        rom = extract_via_decb_entry(dsk, entry)
        if rom is not None:
            return rom

    rom = extract_via_signature_scan(dsk)
    if rom is not None:
        return rom

    raise ValueError("SDC-DOS.ROM not found in SETUP.DSK")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract SDC-DOS.ROM from a CoCoSDC SETUP.DSK image."
    )
    parser.add_argument("setup_dsk", type=Path, help="Path to SETUP.DSK")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("sdc-dos.rom"),
        help="Output ROM path (default: sdc-dos.rom)",
    )
    args = parser.parse_args()

    dsk = args.setup_dsk.read_bytes()
    rom = extract_sdc_dos_rom(dsk)
    args.output.write_bytes(rom)
    print(f"Wrote {len(rom)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
