# Copico 12-in-1 Master Hat: Hardware Assembly Guide
**Date: May 2026 — Verified against PCB Netlist (Hat-Fuji-40C v1)**

This guide provides the complete Bill of Materials (BOM) and wiring instructions for the **12-in-1 Master Hat** built on the **Centipede 32z** board.

> [!IMPORTANT]
> This hardware is designed to work in tandem with the [Firmware Architecture Plan](./firmware_plan.md).
> Ensure all power is disconnected from the Centipede and CoCo before soldering.

> [!CAUTION]
> All GPIO assignments are **verified against** [schematic_copico10in1-fixed_2026-05-24.png](schematic_copico10in1-fixed_2026-05-24.png) (PortaCoco 10-in-1 hat, rev 2.12). See [pinmap.md](pinmap.md). An older duplicate section with wrong SD/SPI pins was removed from this file.

## Bill of Materials (BOM)

### Core Components
*   **[ ] 1x Centipede 32z Board**
*   **[ ] 2x 20-pin Female Headers (2x10, 2.54mm pitch)** (J3 and J4 slots)
*   **[ ] 1x MicroSD Card Module** (Mini PCB — GUIMA edition as per BOM)
*   **[ ] 1x PCM5102A I2S DAC Module** (Orchestra-90 / Speech+Sound output)
*   **[ ] 1x HW-044 TTL-to-RS232 Module** (MAX3232-based, DB9 connector)
*   **[ ] 1x ESP32-C3 Super Mini Module** (FujiNet / WiModem / CoCoSDC coprocessor)

### Connectors & Switches
*   **[ ] 1x HD15 Female VGA Connector** (KH-HDR15P-F3.08 footprint)
*   **[ ] 2x RCA Jacks** (Stereo audio out: LEFT_RCA, RIGHT_RCA)
*   **[ ] 1x Side-Mounted Push Button (4.5x4.5, 3-pin)** (CoCoSDC Disk Swap)
*   **[ ] 1x 5mm LED** (CoCoSDC Activity Indicator)

### Passive Components
*   **[ ] 2x 100Ω Resistors** (R1, R2 — VGA Sync lines: HSync, VSync)
*   **[ ] 3x 470Ω Resistors** (R3, R4, R5 — VGA Red, Green, Blue)
*   **[ ] 1x 330Ω Resistor** (R6 — Status LED current limiter)
*   **[ ] 2x 1kΩ Resistors** (R7, R8 — Audio DAC output mixing)
*   **[ ] 1x 10kΩ Trimpot** (R9 — Volume Control)
*   **[ ] 1x 10µF Electrolytic Capacitor** (C1 — Audio coupling/SND line)
*   **[ ] 1x 1000µF Electrolytic Capacitor** (C2 — 5V rail power stabilizer)
*   **[ ] 1x 100nF Ceramic Capacitor** (C4_104 — ESP32 3.3V noise filter)

---

## Verified GPIO Map (RP2350 Centipede 32z)

### J3 Header (2x10, 2.54mm)
| J3 Pin | Net Name | GPIO | Function |
| :--- | :--- | :--- | :--- |
| 1 | VGA_GREEN_G8 | **G8** | WordPak VGA Green -> R4 (470Ω) -> VGA Pin 2 |
| 2 | VCC | 3.3V | Power Rail |
| 3 | VGA_BLUE_G9 | **G9** | WordPak VGA Blue -> R5 (470Ω) -> VGA Pin 3 |
| 4 | VCC | 3.3V | Power Rail |
| 5 | Sound_BCK_G10 | **G10** | I2S DAC Bit Clock |
| 6 | GND | GND | Ground |
| 7 | Sound_DIN_G11 | **G11** | I2S DAC Data In |
| 8 | GND | GND | Ground |
| 9 | Sound_LCK_G12 | **G12** | I2S DAC LR Clock |
| 10 | GND | GND | Ground |
| 11 | TTL_TX_G13 | **G13** | RS-232 UART TX |
| 12 | VGA_RED_G14 | **G14** | WordPak VGA Red -> R3 (470Ω) -> VGA Pin 1 |
| 13 | TTL_RX_G15 | **G15** | RS-232 UART RX |
| 14 | VGA_VSync_G16 | **G16** | WordPak VGA VSync -> R1 (100Ω) -> VGA Pin 14 |
| 15 | SDC_LED_G17 | **G17** | Status LED -> R6 (330Ω) -> LED |
| 16 | VGA_HSync_G18 | **G18** | WordPak VGA HSync -> R2 (100Ω) -> VGA Pin 13 |
| 17 | SDC_Button_G19 | **G19** | SDC Swap Button (active LOW) |
| 18 | RESET | RESET | Hardware Reset Pad |
| 19 | GND | GND | Ground |
| 20 | +5V | 5V | Power Rail (also C2 1000µF) |

### J4 Header (2x10, 2.54mm) — ESP32 SPI Coprocessor Link
| J4 Pin | Net Name | GPIO | Function |
| :--- | :--- | :--- | :--- |
| 7 | ESP_C3_CS_G23 | **G23** | SPI CS -> ESP32 GPIO4 |
| 9 | ESP_C3_MISO_G24 | **G24** | SPI MISO <- ESP32 GPIO0 |
| 10 | SND | SND | Audio SND line (C1 coupling cap) |
| 15 | ESP_C3_SCK_G30 | **G30** | SPI SCK -> ESP32 GPIO3 |
| 16 | ESP_C3_MOSI_G31 | **G31** | SPI MOSI -> ESP32 GPIO1 |

### ESP32-C3 Super Mini GPIO Map
| ESP32 Pin | Net Name | Function |
| :--- | :--- | :--- |
| GPIO0 / ADC1-0 | ESP_C3_MISO_G24 | SPI MISO <- RP2350 G24 |
| GPIO1 / ADC1-1 | ESP_C3_MOSI_G31 | SPI MOSI -> RP2350 G31 |
| GPIO2 / A0 | (Spare) | — |
| GPIO3 / A1 | ESP_C3_SCK_G30 | SPI SCK <- RP2350 G30 |
| GPIO4 / A2 | ESP_C3_CS_G23 | SPI CS <- RP2350 G23 |
| GPIO5 / A3 | SDC_MOSI | SD Card MOSI |
| GPIO6 / SDA | SDC_MISO | SD Card MISO |
| GPIO7 / SCL | SDC_SCK | SD Card CLK |
| GPIO8 / SCK | (Internal pull-up boot) | |
| GPIO9 / MISO | (Boot mode / spare) | |
| GPIO10 / MOSI | SDC_CS | SD Card Chip Select |
| GPIO20 / RX | (Internal UART RX) | |
| GPIO21 / TX | (Internal UART TX) | |
| 5V pin (8) | +5V | Power from Centipede |
| GND pin (7) | GND | Ground |
| 3.3V pin (6) | +3.3V (via C4_104) | Clean 3.3V for SD Card |

---

## Step-by-Step Assembly

### Phase 1: Header Installation & Power
1.  Solder 2x10 female headers into **J3** and **J4**.
2.  Solder **C2 (1000µF)** across **J3 Pin 20 (+5V)** and **J3 Pin 10 (GND)** for WiFi power stability.
3.  Solder **C4_104 (100nF)** across the ESP32 module's **5V** and **GND** pins.

### Phase 2: MicroSD (CoCoSDC, via ESP32)
> [!NOTE]
> The MicroSD is connected **directly to the ESP32-C3**, not the RP2350. This allows the ESP32 to serve sectors independently.
*   VCC (3.3V) -> ESP32 3.3V rail | GND -> GND
*   MOSI -> ESP32 **GPIO5**
*   MISO -> ESP32 **GPIO6**
*   SCK (CLK) -> ESP32 **GPIO7**
*   CS -> ESP32 **GPIO10**

### Phase 3: I2S DAC (Orchestra-90 / Speech & Sound)
*   VIN -> **J3 Pin 20 (+5V)** | GND -> GND | SCK -> GND (tie low)
*   BCK -> **J3 Pin 5 (G10)**
*   DIN -> **J3 Pin 7 (G11)**
*   LCK -> **J3 Pin 9 (G12)**
*   R (Right audio) -> R7 (1kΩ) -> RIGHT_RCA (+)
*   L (Left audio) -> R8 (1kΩ) -> LEFT_RCA (+)
*   R7+R8 junction -> R9 (10kΩ Trimpot, wiper) -> GND (Volume control)
*   SND net (C1 10µF coupling) -> J4 Pin 10

### Phase 4: WordPak VGA (SuperSprite FM+)
*   **GREEN** -> **J3 Pin 1 (G8)** -> R4 (470Ω) -> VGA Pin 2
*   **BLUE** -> **J3 Pin 3 (G9)** -> R5 (470Ω) -> VGA Pin 3
*   **RED** -> **J3 Pin 12 (G14)** -> R3 (470Ω) -> VGA Pin 1
*   **VSync** -> **J3 Pin 14 (G16)** -> R1 (100Ω) -> VGA Pin 14
*   **HSync** -> **J3 Pin 16 (G18)** -> R2 (100Ω) -> VGA Pin 13
*   VGA Pins 5, 6, 7, 8, 10 -> GND

### Phase 5: RS-232 (HW-044 Module)
*   TTL_TX -> **J3 Pin 11 (G13)** | TTL_RX -> **J3 Pin 13 (G15)**
*   VCC -> 3.3V | GND -> GND

### Phase 6: Status LED & SDC Button
*   **Status LED**: **J3 Pin 15 (G17)** -> R6 (330Ω) -> LED(+) -> LED(-) -> GND
*   **SDC Button**: **J3 Pin 17 (G19)** -> Switch Pin 2 | Switch Pin 1 -> GND

### Phase 7: ESP32-C3 Super Mini (SPI Coprocessor)
> [!CAUTION]
> These pin assignments are **critical**. Swapping MISO/MOSI or CS/SCK will prevent communication between the RP2350 and ESP32.
*   **CS** : ESP32 **GPIO4** <-> **J4 Pin 7 (G23)**
*   **MISO**: ESP32 **GPIO0** <-> **J4 Pin 9 (G24)**
*   **SCK** : ESP32 **GPIO3** <-> **J4 Pin 15 (G30)**
*   **MOSI**: ESP32 **GPIO1** <-> **J4 Pin 16 (G31)**
*   5V -> J3 Pin 20 (+5V) | GND -> J3 Pin 10 (GND)

---
> [!TIP]
> **First Power-On Check**: Continuity test between 3.3V and GND **before** plugging into the CoCo. Then verify the ESP32 serial output at 115200 baud shows `ESP32-C3 Coprocessor Starting...`

See also: [ecosystem.md](ecosystem.md), [pinmap.md](pinmap.md), [firmware_plan.md](firmware_plan.md).
