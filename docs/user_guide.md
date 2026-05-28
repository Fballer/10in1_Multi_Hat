# Copico 12-in-1 Master Hat: User Guide
**Date: May 11, 2026**

Welcome to the Copico 12-in-1 Master Hat! This expansion seamlessly turns your Tandy Color Computer into a powerhouse equipped with networking, SD card storage, advanced audio, and high-resolution MSX2+ graphics.

This guide explains how to use the **CoPico X-BIOS BIOS**, the native configuration menu that appears when you boot the CoCo.

---

## 1. The Main Menu

When you power on your CoCo with the Master Hat attached, you are greeted by the **CoPico X-BIOS BIOS Main Menu**. This screen provides an overview of your current hardware configuration and lets you dynamically swap emulated cartridge modes on the fly.

### Dynamic Configuration Toggles
The bottom half of the screen lists the available configuration toggles. Use the keyboard to cycle through the options for each category.

*   `[A]` **Audio**: Cycles between:
    *   **SPEECH/SOUND CARD** (Tandy Speech & Sound Pak emulation)
    *   **ORCHESTRA 90** (High-quality stereo 8-bit DAC emulation)
    *   **REGULAR COCO** (No audio expansion)
*   `[V]` **Video**: Cycles between:
    *   **WORDPAK 2+** (V9958 MSX2+ Graphics)
    *   **SUPER SPRITE FM** (TMS9918 + OPL2 Audio)
    *   **REGULAR COCO** (Native VDG only)
*   `[C]` **Comm**: Cycles between:
    *   **WIMODEM (RS232 PAK)** (WiFi AT-Command Modem)
    *   **RS-232 PAC** (Legacy Serial Communication)
    *   **FUJINET** (Network drive & printing emulator)
    *   **REGULAR COCO** (No communication expansion)
*   `[D]` **Disk**: Cycles between:
    *   **COCOSDC (SD)** (SD Card floppy emulation)
    *   **FUJINET** (Network drive emulation)
    *   **REGULAR COCO** (Native disk controller only)
*   `[R]` **RTC**: Toggles the Real-Time Clock on/off. (When ON, emulates a Disto 4-N-1 MSM5832 RTC, providing time to OS-9/NitrOS-9 automatically upon boot).

*Note: The Comm and Disk options for FujiNet are internally linked. Selecting FujiNet for Disk will automatically switch Comm to FujiNet as well, as they share the same backend architecture.*

### Booting the CoCo
Once you have selected your desired hardware configuration, press `[ENTER]` to lock in the settings and start the CoCo into BASIC or OS-9.

---

## 2. The Advanced Options Menu

If you press `[O]` on the Main Menu, you will be taken to the **Advanced Options** screen. This area provides diagnostic utilities and configuration tools for the RP2350 and ESP32 co-processors.

At the bottom of this screen, you can see real-time status readouts for the **ESP32 Firmware Version** and the **SD Card** capacity.

### Sub-Menus
From the Advanced Options screen, press the corresponding number key to enter a utility:

*   `[1]` **WIFI CONFIGURATION**
    *   Scan for available wireless networks and join them directly from your CoCo.
    *   Use `[S]` to scan again, or `[J]` to join the highlighted network (you will be prompted to enter a password).
*   `[2]` **ALT ROM LOADER**
    *   Browse the `/ROMS` folder on your inserted SD card.
    *   Use `[L]` to load a custom `.ROM` file into the $C000-$DFFF memory space, overriding the default system ROMs. Use `[E]` to eject the loaded ROM.
*   `[3]` **SNIFFER DIAGNOSTICS**
    *   Displays real-time technical metrics about the RP2350 hardware, including the status of the PIO (Programmable I/O) cores and DMA audio channels.
    *   Use `[R]` to reset the internal bus error counters.
*   `[4]` **ESP32 BRIDGE STATUS**
    *   Displays the health of the high-speed SPI link between the RP2350 and the ESP32-C3 network processor.
    *   Use `[P]` to send a ping command to verify connectivity.
*   `[5]` **CLEAR WIFI SETTINGS**
    *   Safely erase your saved WiFi network and password from the ESP32's flash memory.
*   `[6]` **TIME ZONE CONFIGURATION**
    *   Set your local time zone so the Network Time Protocol (NTP) sync provides the correct local time to the RTC.
    *   The top of the screen displays the currently selected time zone.
    *   Use keys `[1]` through `[5]` to select from UTC, EST, CST, MST, or PST. The RTC will instantly update and provide this time to OS-9.

### Returning to the Main Menu
At any time within the Options Menu or its Sub-Menus, press `[X]` to cancel your current action and return to the previous screen.
