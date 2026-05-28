# Firmware strategy

## Why this repo exists

Hardware bring-up on CoCo 2 + Centipede 32z showed:

- **Bonobo `centipede-watcher`** (`watcher-march.cpp`) serves cart ROM correctly.
- **Arduino PlatformIO** RP2350 path (`setup1`/`loop1`, PIO bus loop) produced checkerboard/garbage on the same wiring.

This repo standardizes on the Bonobo bare-metal multicore engine for RP2350 CoCo bus work.

## Targets

### RP2350 — `firmware/rp2350/`

- **`copico-watcher`**: CoPico X-BIOS cart (`COPICO_XBIOS_ROM=1`), early bus before USB init.
- **`centipede-watcher`**: Disk BASIC regression ROM (`disk11_rom.c`).

Flash via **BOOTSEL + UF2 drag**. `pio upload` / 1200-baud reset is unreliable on Centipede.

### X-BIOS — `firmware/bios/`

6809 menu ROM built with `build_bios.py` (needs `lwasm` from `lwtools-4.21` sibling). Output `xbios_rom.h` is included by `copico-watcher`.

### ESP32 — `firmware/esp32/`

PlatformIO project for WiModem, SD card, SPI bridge to RP2350. Unchanged toolchain from legacy repo.

## Porting from legacy

| Legacy (Arduino) | Port to |
|------------------|---------|
| `boot_menu.cpp` | SCS writes in Bonobo foreground loop |
| `io_dispatch.cpp` | Address decode hooks in `watcher-march.cpp` |
| `hat_config.cpp` | EEPROM + `$FF7F` apply |
| `esp32_bridge.cpp` | Core0 task while core1 runs `Engine0` |

Source copies are in `firmware/reference/` for convenience.

## Legacy repo

Full phase 1–9 history: [copico-12in1-master-hat](https://github.com/Fballer/copico-12in1-master-hat), branch `testing-and-integrating`, tag **`v0.9-legacy-arduino-bringup`**.
