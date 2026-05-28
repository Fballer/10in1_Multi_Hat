# Copico 12-in-1 Master Hat: Firmware Architecture & Implementation Plan
**Date: May 2026 — Updated with verified PCB netlist (Hat-Fuji-40C v1)**

This document details the software architecture and development roadmap for the **12-in-1 Master Hat**. It leverages the RP2350 dual-core processor to emulate an entire suite of CoCo expansions.

> [!IMPORTANT]
> This plan assumes the hardware is built according to the [Hardware Assembly Guide](build_guide.md) and [pin map](pinmap.md) (see [schematic](schematic_copico10in1-fixed_2026-05-24.png)).

## 1. System Philosophy: The "CoPico X-BIOS" Expansion
The RP2350 acts as a real-time bridge between the CoCo's 6809 bus and modern peripherals. By dynamically swapping PIO state machines and memory maps, a single physical board becomes 12 different classic cartridges.

> [!IMPORTANT]
> **LOCAL SPECIFICATIONS FIRST**: The technical "guts" (registers, ports, and protocols) for these modules have been saved directly to this project. **Reference these local files first** during the firmware build to ensure we stay offline and fast.

| Emulation Mode | Core Hardware | Technical Reference / Repository |
| :--- | :--- | :--- |
| **Orchestra-90** | 8-bit Mono DAC | [Archive: Orch-90 Manual](https://colorcomputerarchive.com/repo/Documents/Manuals/Hardware/Orchestra-90%20(Tandy).pdf) \| **[LOCAL: Peripherals Specs](../references/peripherals_specs.md)** |
| **Speech & Sound** | AY-3-8910 (PSG) | [GitHub: AY-3-8910 Emulation](https://github.com/mamedev/mame/blob/master/src/devices/sound/ay8910.cpp) \| [GitHub: LibAYemu](https://github.com/vsergeev/libayemu) |
| **RS-232 Pak** | ACIA 6551 | [Reference: 6551 Datasheet](https://www.westerndesigncenter.com/wdc/documentation/w65c51n.pdf) \| **[LOCAL: Peripherals Specs](../references/peripherals_specs.md)** |
| **WordPak 2+** | Yamaha V9958 | [Source: CoCoLoCo WordPak-2](https://sites.google.com/site/tandycocoloco/wordpak-2) \| **[LOCAL: V9958 Specs](../references/wordpak2/v9958_specs.md)** |
| **CoCoSDC** | Custom FDC | [Blog: CoCoSDC Official](http://cocosdc.blogspot.com/) \| **[LOCAL: CoCoSDC Specs](../references/cocosdc/cocosdc_specs.md)** |
| **FujiNet** | ESP32 Networking | [GitHub: FujiNet PlatformIO](https://github.com/FujiNetWIFI/fujinet-platformio) \| [GitHub: fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware) |
| **DriveWire 4** | Virtual Serial Disk | [Source: DriveWire 4](http://www.drivewire4.com/) \| [GitHub: pyDriveWire](https://github.com/n64764/pyDriveWire) |
| **WiModem** | AT Command Modem | [Reference: Zimodem Firmware](https://github.com/boisy/Zimodem) \| [GitHub: GuruModem](https://github.com/pizet/GuruModem) |
| **Real-Time Clock** | MSM5832 Emulation | [Library: DS3231 I2C](https://github.com/adafruit/Adafruit_DS3231) \| [GitHub: CoCo RTC Driver](https://github.com/boisy/NitrOS-9/blob/master/sys/modules/rtc3231.as) |
| **SuperSprite FM+** | TMS9918 + OPL2 | [Reference: TMS9918 Datasheet](https://www.msxarchive.nl/pub/msx/mirrors/hansa.xs4all.nl/tms9918.pdf) \| [GitHub: OPL2 Emulator](https://github.com/D-S-X/OPL2-Emulation) |
| **CoPico X-BIOS** | Injected 6809 Code | [Source: FujiNet Config Application](https://github.com/FujiNetWIFI/fujinet-config) \| [GitHub: Mega-Cart](https://github.com/sublogic/MegaCart) |

---

## 3. Development Roadmap & Success Probability

We will develop each module in isolation, hardcoding the RP2350 for testing before integrating them into the master Boot Menu.

### Phase 1: Orchestra-90 Emulation (Success: 95%)
*   Trap `$FF7A/B` and push to I2S DAC. Simple bus-sniffing "quick win."

### Phase 2: Speech and Sound Pak (Success: 85%)
*   Real-time synthesis of PSG waveforms in the RP2350 CPU core.
*   **CoCo 3 / 6309 / GIME-X Auto-Correction**: Detect host E-Clock frequency in real-time (0.89MHz to 3MHz+). Normalizes PSG pitch and ensures bus-trapping logic remains stable during high-speed operation.

### Phase 3: RS-232 Pak (Success: 90%)
*   Emulate ACIA registers and route to physical MAX3232 pins.

### Phase 4: WordPak 2+ (V9958 VGA) (Success: 75%)
*   **The "Boss Fight"**: Emulate MSX2+ graphics and output 3-bit RGB VGA. Requires intense PIO optimization. Master this first to provide the video foundation for the SuperSprite.

### Phase 5: The Native Boot Menu (Success: 85%)
*   Inject 6809 assembly into CoCo RAM to display a selection menu on the native screen.

### Phase 6: CoCoSDC (Local Storage) (Success: 80%)
*   Implement FAT32 support and precise floppy controller timing.

### Phase 7: Wireless Suite (ESP32-C3)
*   **7A. WiFi Connection** (95%)
*   **7B. Wireless WiModem** (90%): Bridge RS-232 to Telnet via ESP32.
*   **7C. FujiNet / DriveWire** (80%): Stream sectors over the local network.
*   **7D. Cloud CoCoSDC** (75%): Fetch `.DSK` files directly from the Color Computer Archive.

### Phase 8: SuperSprite FM+ (Success: 70%)
*   **Advanced Feature**: Leverage the V9958 VGA core from Phase 4 and add software FM synthesis (OPL2 emulation). This turns the CoCo into an FM-synth music machine.

---

## 4. Boot Menu & ROM Management
*   **Native VDG Menu**: The primary menu runs on the CoCo's original green/black screen.
*   **BIOS Shared Memory**: The RP2350 exposes a shared memory block starting at `$D800` for the 6809 to read real-time status strings dynamically injected by the ESP32 and RP2350:
    *   `$D800`: WiFi Status (max 32 bytes, null-terminated)
    *   `$D820`: ESP32 Firmware Version (max 32 bytes, null-terminated)
    *   `$D840`: SD Card Status (max 32 bytes, null-terminated)
    *   `$D860`: Time Zone Setting (max 32 bytes, null-terminated)

> [!NOTE]
> Status buffers were relocated from `$C800` to `$D800` (Phase 8) to prevent conflicts with the BIOS ROM code space (`$C000-$DFFF`).

*   **Hardware Registers**:
    *   `$FF50-$FF51`: Emulated MSM5832 RTC interface (compatible with Disto 4-N-1 and NitrOS-9).
    *   `$FF70`: Boot Menu Audio selection register.
    *   `$FF71`: Boot Menu Video selection register.
    *   `$FF72`: Boot Menu Disk selection register.
    *   `$FF73`: Boot Menu Comm selection register.
    *   `$FF75`: Time Zone index register (Write 0-17 to select timezone; ESP32 applies POSIX TZ string).
    *   `$FF7F`: Mode-switch register (Write EmulatorMode value to switch; Write `$55` from Boot Menu to save+switch).

*   **RP2350 ↔ ESP32 SPI Link (Verified PCB Netlist)**:
    *   **CS** : RP2350 **G23** ↔ ESP32 **GPIO4** (J4 Pin 7)
    *   **MISO**: RP2350 **G24** ↔ ESP32 **GPIO0** (J4 Pin 9)
    *   **SCK** : RP2350 **G30** ↔ ESP32 **GPIO3** (J4 Pin 15)
    *   **MOSI**: RP2350 **G31** ↔ ESP32 **GPIO1** (J4 Pin 16)
*   **Personality Swapping**: Selecting a menu item triggers a "hot-swap" of the RP2350's memory mapping.

### ROM Management Strategy (16KB Flash Bank Architecture)
The CoPico X-BIOS leverages a robust 256KB flash bank (12 slots of 16KB) for instant-on performance and hardware-accurate bank switching:

1.  **Slot 0: CoPico X-BIOS**: (Embedded). The system's primary control menu.
2.  **Slot 1: SDC-DOS**: (Auto-installed). Required for CoCoSDC disk emulation. Installed from SD card on first boot to maintain legal compliance.
3.  **Slot 2: FujiNet BIOS**: (Embedded). Networking ROM for the ESP32 bridge.
4.  **Slot 3: RS-232 Pak ROM**: (Embedded). Deluxe RS-232 ROM for terminal communication.
5.  **Slots 4-11: CoCoSDC Banks**: Emulates the 8-bank hardware array. Bank 0 (Slot 4) defaults to SDC-DOS; Bank 1 (Slot 5) defaults to Disk BASIC 1.1.

*   **Recovery Mode**: Powering on while holding **G19** (Dual Button) forces an immediate bypass to the CoPico X-BIOS (Slot 0), allowing recovery from a soft-bricked state.
*   **Bank Switching**: Responds to `RUN @n` commands via the PIO bus sniffer, mirroring the real CoCoSDC hardware behavior.

### Bus Timing: Hybrid Fast-Map I/O Dispatch

> [!IMPORTANT]
> The CoCo bus at **2.86 MHz** (GIME-X + 6309 turbo) gives only **175 ns** to respond to a read. The original firmware used a C++ `if/else` chain that consumed ~40 ns in branch comparisons alone. Combined with slow `gpio_set_dir_out_masked()` calls, worst-case latency was ~500 ns — a guaranteed failure at turbo speeds.

**Solution (Phase 8 Fortification):**

1.  **PIO Read-Response SM** (`coco_bus_read`): A third PIO state machine on PIO0 handles the entire GPIO turnaround (set output → drive data → wait E-clock fall → tri-state) in hardware. This replaced ~150 ns of C++ MMIO calls with a single 7 ns FIFO write (`BUS_RESPOND()` macro).

2.  **Hybrid Fast-Map**: The `if/else` dispatch chain was replaced with:
    *   **ROM Shadow** (`$C000-$DFFF`): A single function pointer (`rom_read_handler`). One comparison + one indirect call = ~20 ns.
    *   **I/O Space** (`$FF00-$FFFF`): A 256-entry function pointer table (`io_read_table[]` / `io_write_table[]`), indexed by `addr & 0xFF`. One array access + null check + indirect call = ~7 ns.
    *   Tables are rebuilt on every mode switch via `rebuild_io_tables()`.

| CoCo Speed | Budget | Estimated Latency | Margin |
| :--- | :--- | :--- | :--- |
| 1.0 MHz (Stock) | 500 ns | ~120 ns | ✅ 380 ns |
| 1.79 MHz (Turbo) | 279 ns | ~120 ns | ✅ 159 ns |
| 2.86 MHz (GIME-X) | 175 ns | ~120 ns | ✅ **55 ns** |

*   **Source files**: `io_dispatch.h`, `io_dispatch.cpp`, `coco_bus.pio` (`coco_bus_read` program).
*   **Memory cost**: 256 × 4 bytes × 2 tables = **2 KB RAM** (negligible on RP2350's 520 KB).

### VGA Jitter Elimination (DMA Interrupt-Driven Double Buffer)

> [!NOTE]
> The VGA signal is generated entirely by PIO1 (3 state machines: HSYNC, VSYNC, RGB) and fed by a DMA chain. The CPU is **never** in the VGA timing path.

**Problem**: The original `tick()` function used a blocking `while()` loop that spun until the DMA finished reading a scanline buffer. If the CPU was busy (audio synthesis, bus handling), the loop stalled, causing visible jitter.

**Solution**: A DMA completion interrupt (`DMA_IRQ_0`) fires when each scanline transfer finishes. The ISR:
1.  Atomically swaps the active buffer index (0 ↔ 1)
2.  Points the DMA at the new buffer
3.  Increments a `_scanlines_completed` counter

`tick()` checks the counter. If > 0, it renders the next scanline into the inactive buffer. If `tick()` falls behind (CPU busy), the DMA simply replays the last buffer — a graceful degradation (repeated scanline) instead of signal corruption.

*   **Source files**: `vga_driver.h`, `vga_driver.cpp`, `vga_pio.pio`
*   **ISR**: `VgaDriver::on_dma_complete()` (static, registered on `DMA_IRQ_0`)
*   **CPU impact**: Zero. VGA signal is 100% hardware-driven.

---

### Multi-Pak Interface (MPI) Compatibility
While the 10-in-1 Hat cannot connect directly to a physical floppy drive, it **can** operate inside a Tandy Multi-Pak Interface alongside a real physical Disk Controller. 
*   **Hardware Handshake**: The Hat relies on the `CTS*` (Cartridge Select) signal. When in an MPI, the MPI hardware only sends the `CTS*` signal to the currently selected slot. 
*   **Result**: The Copico will remain completely "silent" on the bus until the CoCo switches to its specific MPI slot, allowing it to perfectly co-exist with real floppy controllers or other physical cartridges without bus conflicts.

*   **Physical Override**: The SPDT switch (G20) allows forcing a "Safe Mode" boot to the menu if a firmware setting gets stuck.
