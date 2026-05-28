# Yamaha V9958 Technical Summary (WordPak 2+ Integration)

## 1. I/O Port Mapping (CoCo Specific)
The WordPak 2+ maps the V9958 registers to the following I/O addresses on the TRS-80 Color Computer:
*   **$FF78**: VRAM Data (Read/Write)
*   **$FF79**: Register Write / Status Read
*   **$FF7A**: Palette Write / VRAM Data (Extra)
*   **$FF7B**: Indirect Register Write / Register Select

## 2. VDP Register Map
*   **Mode Registers (R#0–R#1)**: Set the primary display mode (Screen 0-12).
*   **VRAM Pointer (R#14)**: Bank selection for 128KB VRAM. Access via $FF78 occurs in 16KB pages.
*   **Scrolling (R#25–R#27)**: Horizontal and Vertical scroll offsets (MSX2+ specific).
*   **Color Palette**: Accessed via $FF7A. Each color index (0-15) has a 9-bit RGB value (3 bits per channel).

## 3. Command Engine (R#32–R#46)
Hardware-accelerated graphics commands:
*   **HMMM**: High-speed VRAM-to-VRAM copy (BitBlt).
*   **LMMM**: Logical VRAM-to-VRAM copy (with logical ops like AND, OR, XOR).
*   **HMMV**: High-speed VRAM fill.
*   **LINE**: Hardware line drawing engine.
*   **SRCH**: Pixel color search (useful for collision detection).

## 4. Status Registers (S#0–S#9)
*   **S#0**: Status register. Contains the CE (Command Executing) bit and TR (Data Ready) bit.
*   **S#1**: Error status and V-Sync detection.

## 5. VRAM Layout (128KB)
*   Linear addressing from $00000 to $1FFFF.
*   The CPU accesses this through the $FF78 port using the page register R#14.
