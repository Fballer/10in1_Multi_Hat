# CoCoSDC Technical Summary (Emulation Specifications)

## 1. Memory-Mapped Registers ($FF40 - $FF4B)
The CoCoSDC emulates a standard FDC at $FF40-$FF47 but uses $FF48-$FF4B for "Extended SDC Mode" operations.

| Address | Function (Extended Mode) |
| :--- | :--- |
| **$FF40** | **Control Register**: Write `$43` to activate Extended SDC Mode. |
| **$FF48** | **Command/Status**: Write commands (Read, Write, Mount); Read status bits. |
| **$FF49** | **LSN High**: Bits 16-23 of the 24-bit Logical Sector Number. |
| **$FF4A** | **LSN Mid / Data Port 1**: Bits 8-15 of LSN / 8-bit data transfer port. |
| **$FF4B** | **LSN Low / Data Port 2**: Bits 0-7 of LSN / 8-bit data transfer port. |

## 2. Status Register Bits ($FF48 Read)
*   **Bit 0 (BUSY)**: 1 = Controller is busy. Wait for 0 before sending a new command.
*   **Bit 1 (READY)**: 1 = Data is ready in the buffer (for Reads) or buffer is ready (for Writes).
*   **Bit 7 (ERROR)**: 1 = Operation failed.

## 3. Command Set ($FF48 Write)
*   **$80**: Read 256-byte sector from current LSN.
*   **$A0**: Write 256-byte sector to current LSN.
*   **$E0**: Mount Drive 0. Requires sending a 256-byte packet with the filename string.
*   **$E1**: Mount Drive 1.

## 4. Block Transfer Protocol
1.  Set the 24-bit LSN using $FF49, $FF4A, $FF4B.
2.  Issue command $80 or $A0.
3.  Wait for Bit 1 of $FF48 to go high.
4.  Transfer 256 bytes by alternating reads/writes between **$FF4A** and **$FF4B**. 
    *   *Note: In RP2350 emulation, we can detect the alternating access to handle the byte streaming from the internal SD buffer.*
