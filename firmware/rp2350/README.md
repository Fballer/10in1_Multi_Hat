# RP2350 — Bonobo CoCo bus engine

Based on Erico's **centipede-watcher** (`watcher-march.cpp`) from Bonobo v3.1 Centipede.

## Build

Requires Pico SDK (e.g. from PlatformIO earlephilhower core):

```bash
export PICO_SDK_PATH="$HOME/.platformio/packages/framework-arduinopico/pico-sdk"
cmake -B build
cmake --build build --target copico-watcher    # CoPico X-BIOS (shipping)
cmake --build build --target centipede-watcher # Disk BASIC regression
```

Rebuild X-BIOS first if `firmware/bios/xbios.asm` changed:

```bash
cd ../bios && python3 build_bios.py
```

## Flash

1. Hold **BOOTSEL**, plug Centipede USB, release.
2. Copy `build/copico-watcher.uf2` to the RP2350 volume.
3. Reset CoCo — X-BIOS menu should appear immediately (early-bus build).

## CoPico patches vs upstream Bonobo

- `COPICO_XBIOS_ROM`: includes `../bios/xbios_rom.h` instead of `disk11_rom.c`
- Early `Engine0::Run()` before USB/blink delays when `COPICO_XBIOS_ROM` is set
