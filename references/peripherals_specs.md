# Peripheral Technical Summary: Orchestra-90 and 6551 ACIA

## 1. Orchestra-90 (Sound Pak)
The Orch-90 is a dual 8-bit DAC system. It is extremely simple to emulate.
*   **$FF7A**: Left Channel DAC (Write 8-bit signed/unsigned byte).
*   **$FF7B**: Right Channel DAC (Write 8-bit signed/unsigned byte).
*   **Sampling**: There is no hardware timer. The CoCo CPU manually POKEs values to these ports to create waveforms. 
*   **RP2350 Implementation**: We trap the write to $FF7A/B and immediately push the value to the I2S DMA buffer.

---

## 2. 6551 ACIA (RS-232 Pak)
The ACIA is the standard serial controller for the CoCo. It is mapped to **$FF68-$FF6B**.

| Address | Register | Function |
| :--- | :--- | :--- |
| **$FF68** | **Data** | Read (Rx) / Write (Tx) serial data. |
| **$FF69** | **Status** | Read IRQ, Overrun, Framing Error, Tx/Rx buffer status. |
| **$FF6A** | **Command** | Control parity, interrupts, and DTR/RTS lines. |
| **$FF6B** | **Control** | Set Baud Rate (50 to 19200 bps), word length, and stop bits. |

### **Status Register Bits ($FF69 Read)**
*   **Bit 0 (IRQ)**: 1 = Interrupt has occurred.
*   **Bit 3 (Rx Full)**: 1 = Data received and ready to read from $FF68.
*   **Bit 4 (Tx Empty)**: 1 = Ready to accept new data for transmission.
*   **Bit 5 (DCD)**: Data Carrier Detect status.
*   **Bit 6 (DSR)**: Data Set Ready status.

### **Control Register Bits ($FF6B Write)**
*   **Bits 0-3**: Baud rate selection.
    *   `1110`: 9600 bps
    *   `1111`: 19200 bps
*   **Bits 5-6**: Word length (8, 7, 6, 5 bits).
*   **Bit 7**: Stop bits (1 or 2).

### **RP2350 Implementation**
The RP2350 will bridge these register writes to its internal **Hardware UART0** (connected to the MAX3232 module) or to the **ESP32-C3** for virtual WiModem functionality.
