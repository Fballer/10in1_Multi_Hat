# CoPico project ecosystem

This repo (`10in1_Multi_Hat`) is the **shipping firmware** for the PortaCoco 10-in-1 Centipede 32z hat.

## Recommended Cursor / IDE workspace

Open the parent folder:

```
/Users/macbook/Documents/PlatformIO/Projects/copico/
```

That gives access to build tools and the legacy repo without duplicating them in git.

## Sibling folders (under `copico/`)

| Path | Purpose |
|------|---------|
| **`10in1_Multi_Hat/`** (this repo) | RP2350 Bonobo engine, X-BIOS, ESP32, docs |
| **`copico-12in1-master-hat/`** | **Legacy** — Arduino PlatformIO RP2350 experiments (reference only) |
| **`lwtools-4.21/`** | 6809 assembler (`lwasm`) for CoPico X-BIOS — `firmware/bios/build_bios.py` |
| **`copico-bonobo-main/`** | Upstream Bonobo KiCad + centipede-watcher source (local; vendored into this repo) |

## Authoritative hardware docs (this repo)

| Doc | Use |
|-----|-----|
| [schematic_copico10in1-fixed_2026-05-24.png](schematic_copico10in1-fixed_2026-05-24.png) | **Wiring you built** — PortaCoco 10-in-1 hat (rev 2.12) |
| [pinmap.md](pinmap.md) | GPIO ↔ net names |
| [build_guide.md](build_guide.md) | Assembly BOM and J3/J4 steps (verified section only) |
| [firmware-strategy.md](firmware-strategy.md) | RP2350 = Bonobo; legacy Arduino = reference |

## Build commands

```bash
# X-BIOS ROM (needs lwasm — see lwtools-4.21 sibling)
cd firmware/bios && python3 build_bios.py

# RP2350 CoCo bus (Bonobo copico-watcher)
export PICO_SDK_PATH="$HOME/.platformio/packages/framework-arduinopico/pico-sdk"
cd firmware/rp2350 && cmake -B build && cmake --build build --target copico-watcher

# ESP32 coprocessor
cd firmware/esp32 && pio run
```

## Centipede base board (CoCo bus)

The hat schematic covers J3/J4 peripherals. The **6809 bus** pins are on the Centipede 32z itself — see `firmware/rp2350/watcher-march.cpp` and [pinmap.md](pinmap.md).

## Git

| Repo | GitHub |
|------|--------|
| **10in1_Multi_Hat** (this) | Production |
| **copico-12in1-master-hat** | Legacy — tag `v0.9-legacy-arduino-bringup` |

Siblings `lwtools-4.21` and `copico-bonobo-main` are local support files.
