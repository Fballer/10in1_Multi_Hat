# Arduino RP2350 reference (legacy)

These files were copied from [copico-12in1-master-hat](https://github.com/Fballer/copico-12in1-master-hat) at tag `v0.9-legacy-arduino-bringup`.

**Do not build these as the RP2350 shipping firmware.** Port logic into `firmware/rp2350/watcher-march.cpp` and related Bonobo hooks.

| File | Port target |
|------|-------------|
| `boot_menu.cpp/h` | SCS I/O `$FF70–$FF76`, menu state |
| `io_dispatch.cpp/h` | Peripheral address decode |
| `hat_config.cpp/h` | EEPROM persistence, `$FF7F` apply |
| `esp32_bridge.cpp/h` | Core0 SPI to ESP32 |
| `flash_rom_manager.cpp/h` | Flash ROM bank slots |

See [docs/firmware-strategy.md](../docs/firmware-strategy.md).
