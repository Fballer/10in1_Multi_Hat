# GPIO pin map — verified against schematic & firmware

Source: `docs/schematic_copico10in1-fixed_2026-05-24.png` (PortaCoco 10-in-1 Centipede 32z Hat, rev 2.12) and firmware as of May 2026.

## J3 — RP2350 (2×10)

| J3 pin | Net | GPIO | Firmware |
|--------|-----|------|----------|
| 1 | VGA_GREEN_G8 | G8 | `vga_driver.cpp` `PIN_GREEN` |
| 5 | Sound_BCK_G10 | G10 | `main.cpp` `I2S_BCK` |
| 7 | Sound_DIN_G11 | G11 | `I2S_DIN` |
| 9 | Sound_LCK_G12 | G12 | `I2S_LCK` |
| 11 | TTL_TX_G13 | G13 | `SerialPIO` TX |
| 12 | VGA_RED_G14 | G14 | `PIN_RED` |
| 13 | TTL_RX_G15 | G15 | `SerialPIO` RX |
| 14 | VGA_VSync_G16 | G16 | `PIN_VSYNC` |
| 15 | SDC_LED_G17 | G17 | `PIN_STATUS_LED` |
| 16 | VGA_HSync_G18 | G18 | `PIN_HSYNC` |
| 17 | SDC_Button_G19 | G19 | `PIN_BTN_DUAL` |
| 18 | RESET | — | Hardware reset pad |

## J4 — RP2350 ↔ ESP32-C3 SPI

| J4 pin | Net | RP2350 | ESP32-C3 | Firmware |
|--------|-----|--------|----------|----------|
| 7 | ESP_C3_CS_G23 | G23 | GPIO4 | `esp32_bridge.cpp`, `spi_stream.cpp` |
| 9 | ESP_C3_MISO_G24 | G24 | GPIO0 | |
| 15 | ESP_C3_SCK_G30 | G30 | GPIO3 | |
| 16 | ESP_C3_MOSI_G31 | G31 | GPIO1 | |
| 10 | SND | — | — | Audio coupling (C1), not a GPIO |

## ESP32-C3 — MicroSD (CoCoSDC, not on RP2350)

| Net | ESP32 GPIO | Firmware (`main_esp32.cpp`) |
|-----|------------|----------------------------|
| SDC_MOSI | 5 | `SD_MOSI` |
| SDC_MISO | 6 | `SD_MISO` |
| SDC_SCK | 7 | `SD_SCK` |
| SDC_CS | 10 | `SD_CS` |

## CoCo cartridge bus (Centipede 32z — not on hat schematic)

| Signal | GPIO | Firmware |
|--------|------|----------|
| D0–D7 | 0–7 | `coco_bus.pio` |
| A0–A15 | 32–47 | `coco_bus.pio` |
| R/W | 20 | `PIN_RW` |
| E-clock | 21 | `PIN_E_CLOCK` |

## Modules on schematic

| Block | Interface |
|-------|-----------|
| PCM5102A | I2S G10–G12 → RCA (R7/R8, trimpot R9) |
| HW-044 | TTL G13/G15 → DB9 |
| VGA HD15 | RGB + sync G8, G9, G14, G16, G18 |
| ESP32-C3 Super Mini | SPI to J4; SD on GPIO 5–7, 10 |
| CoCoSDC LED / button | G17, G19 |

RTC is **software-emulated** on the RP2350 (`rtc.cpp`), synced from ESP32 NTP over SPI — no DS3231 on this schematic.
