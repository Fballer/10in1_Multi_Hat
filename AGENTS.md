# Agent instructions — CoPico 10-in-1 Multi Hat

## Workspace

Open **`/Users/macbook/Documents/PlatformIO/Projects/copico/`** (parent folder) so siblings `lwtools-4.21/` and legacy `copico-12in1-master-hat/` are available.

Read **`docs/ecosystem.md`** and **`docs/firmware-strategy.md`** first.

## Hardware truth

- **Wiring / GPIO**: `docs/schematic_copico10in1-fixed_2026-05-24.png`, `docs/pinmap.md`, `docs/build_guide.md` (top section only).
- **CoCo bus** on Centipede 32z — Bonobo pin model in `firmware/rp2350/watcher-march.cpp`.
- **J6**: bridge 2↔3 and 4↔5 for schmitt E/Q path on CoCo 2 bring-up.

## Firmware entry points

| Target | Path | Build |
|--------|------|-------|
| RP2350 (shipping) | `firmware/rp2350/` | `cmake --build build --target copico-watcher` |
| X-BIOS ROM | `firmware/bios/` | `python3 build_bios.py` |
| ESP32-C3 | `firmware/esp32/` | `pio run` |

**Do not** add new CoCo bus logic in Arduino PlatformIO for RP2350. Extend `watcher-march.cpp` / Bonobo engine.

## Legacy reference

Arduino RP2350 code to port lives in:

- **`firmware/reference/`** — boot menu, I/O dispatch, hat config, ESP32 bridge
- **`copico-12in1-master-hat`** (sibling repo) — full phase history

## Do not

- Change GPIO assignments without checking `docs/pinmap.md` and `watcher-march.cpp`.
- Assume MicroSD is on the RP2350 — it is on the ESP32 (GPIO 5–7, 10).
- Commit `firmware/rp2350/build/`, `.pio/`, or regenerated `xbios_rom.h` without rebuilding.
