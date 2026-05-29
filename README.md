# CoPico 10-in-1 Multi Hat

> **Superseded:** Hardware rev 2.12 had incorrect J3/J4 pin assumptions. Active development moved to **[8in1_Multi_HAT](https://github.com/Fballer/8in1_Multi_HAT)** (FIXED J3 header, no VGA). This repo is frozen for reference.

Firmware for the **PortaCoco 10-in-1 Centipede 32z hat** — RP2350 CoCo bus emulation, CoPico X-BIOS, and ESP32-C3 wireless coprocessor.

This repo was the **production line** after initial hardware bring-up. The earlier Arduino PlatformIO RP2350 experiments live in the legacy repo for reference only.

## Legacy reference

| Repo | Purpose |
|------|---------|
| [copico-12in1-master-hat](https://github.com/Fballer/copico-12in1-master-hat) | Phase 1–9 Arduino firmware, boot menu, I/O dispatch — **archived** at tag `v0.9-legacy-arduino-bringup` |

Port code from `firmware/reference/` (copied from legacy) into the Bonobo `copico-watcher` engine as peripherals are integrated.

## Architecture

| Target | Toolchain | Path |
|--------|-----------|------|
| **RP2350 CoCo bus** | CMake + Pico SDK | `firmware/rp2350/` → **`copico-watcher.uf2`** |
| **CoPico X-BIOS ROM** | lwasm (`lwtools-4.21` sibling) | `firmware/bios/` |
| **ESP32-C3 coprocessor** | PlatformIO | `firmware/esp32/` |

RP2350 bus serving uses the **Bonobo centipede-watcher engine** (`watcher-march.cpp`), not Arduino `setup1`/`loop1`. Validated on CoCo 2 + Centipede 32z with J6 schmitt jumpers (2↔3, 4↔5).

## Quick build

```bash
# X-BIOS ROM (needs lwasm on PATH or ../lwtools-4.21 sibling)
cd firmware/bios && python3 build_bios.py

# RP2350 — copico-watcher (X-BIOS cart)
export PICO_SDK_PATH="$HOME/.platformio/packages/framework-arduinopico/pico-sdk"
cd firmware/rp2350
cmake -B build && cmake --build build --target copico-watcher
# Flash: copy build/copico-watcher.uf2 via BOOTSEL

# ESP32 coprocessor
cd firmware/esp32 && pio run
```

## Documentation

- [Project ecosystem](docs/ecosystem.md)
- [Firmware strategy](docs/firmware-strategy.md)
- [GPIO pin map](docs/pinmap.md)
- [Hardware build guide](docs/build_guide.md)
- [Hat schematic](docs/schematic_copico10in1-fixed_2026-05-24.png)

## Integration roadmap

1. **Done:** X-BIOS boot via `copico-watcher` (Bonobo engine + early bus)
2. **Next:** Boot menu I/O (`$FF70–$FF76`, EEPROM) — port from `firmware/reference/`
3. ESP32 SPI bridge on core0
4. Peripherals one at a time (Orch-90, RS-232, CoCoSDC, WordPak, wireless)
