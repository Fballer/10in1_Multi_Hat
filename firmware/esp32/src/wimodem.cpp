#include "wimodem.h"
#include <WiFi.h>
#include <WiFiClient.h>

extern uint8_t modem_rx_fifo[];
extern volatile uint16_t modem_rx_head;
extern volatile uint16_t modem_rx_tail;

extern uint8_t modem_tx_fifo[];
extern volatile uint16_t modem_tx_head;
extern volatile uint16_t modem_tx_tail;

#define ESP_FIFO_SIZE 1024

WiFiClient client;
bool data_mode = false;
String cmd_buffer = "";
unsigned long last_plus_time = 0;
int plus_count = 0;

int modem_available() {
    if (modem_rx_head >= modem_rx_tail) return modem_rx_head - modem_rx_tail;
    return ESP_FIFO_SIZE - modem_rx_tail + modem_rx_head;
}

int modem_read() {
    if (modem_rx_head == modem_rx_tail) return -1;
    uint8_t val = modem_rx_fifo[modem_rx_tail];
    modem_rx_tail = (modem_rx_tail + 1) % ESP_FIFO_SIZE;
    return val;
}

void modem_write(uint8_t c) {
    uint16_t next_head = (modem_tx_head + 1) % ESP_FIFO_SIZE;
    if (next_head == modem_tx_tail) return; // Buffer full
    modem_tx_fifo[modem_tx_head] = c;
    modem_tx_head = next_head;
}

void modem_print(const char* str) {
    while (*str) modem_write(*str++);
}

void wimodem_setup() {
    // Start in AT command mode
    data_mode = false;
    WiFi.mode(WIFI_STA);
}

void process_at_command(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "AT") {
        modem_print("\r\nOK\r\n");
    } 
    else if (cmd.startsWith("AT+WIFI=")) {
        String args = cmd.substring(8);
        int comma_idx = args.indexOf(',');
        if (comma_idx > 0) {
            String ssid = args.substring(0, comma_idx);
            String pass = args.substring(comma_idx + 1);
            WiFi.begin(ssid.c_str(), pass.c_str());
            modem_print("\r\nOK\r\n");
        } else {
            modem_print("\r\nERROR\r\n");
        }
    }
    else if (cmd == "AT+IP?") {
        modem_print("\r\n");
        modem_print(WiFi.localIP().toString().c_str());
        modem_print("\r\nOK\r\n");
    }
    else if (cmd.startsWith("ATDT")) {
        String addr = cmd.substring(4);
        int colon_idx = addr.indexOf(':');
        if (colon_idx > 0) {
            String host = addr.substring(0, colon_idx);
            int port = addr.substring(colon_idx + 1).toInt();
            modem_print("\r\nCONNECTING...\r\n");
            if (client.connect(host.c_str(), port)) {
                modem_print("\r\nCONNECT 115200\r\n");
                data_mode = true;
            } else {
                modem_print("\r\nNO CARRIER\r\n");
            }
        } else {
            modem_print("\r\nERROR\r\n");
        }
    }
    else if (cmd == "ATH") {
        client.stop();
        modem_print("\r\nOK\r\n");
    }
    else if (cmd == "ATZ") {
        client.stop();
        data_mode = false;
        WiFi.disconnect();
        modem_print("\r\nOK\r\n");
    }
    else if (cmd.length() > 0) {
        modem_print("\r\nERROR\r\n");
    }
}

void wimodem_loop() {
    // 1. Handle incoming data from RP2350
    while (modem_available()) {
        char c = (char)modem_read();
        
        if (data_mode) {
            // TCP stream mode
            if (c == '+') {
                unsigned long now = millis();
                if (now - last_plus_time > 1000) plus_count = 0; // Guard time
                plus_count++;
                last_plus_time = now;
                if (plus_count == 3) {
                    data_mode = false;
                    plus_count = 0;
                    modem_print("\r\nOK\r\n");
                }
            } else {
                plus_count = 0;
                if (client.connected()) {
                    client.write(c);
                }
            }
        } else {
            // AT Command Mode
            if (c == '\r' || c == '\n') {
                if (cmd_buffer.length() > 0) {
                    process_at_command(cmd_buffer);
                    cmd_buffer = "";
                }
            } else if (c == 8 || c == 127) { // Backspace
                if (cmd_buffer.length() > 0) {
                    cmd_buffer.remove(cmd_buffer.length() - 1);
                    modem_write(c); modem_write(' '); modem_write(c); // Echo backspace
                }
            } else {
                cmd_buffer += c;
                modem_write(c); // Local echo
            }
        }
    }
    
    // 2. Handle incoming data from WiFi TCP
    if (client.connected()) {
        while (client.available()) {
            char c = client.read();
            if (data_mode) {
                modem_write(c);
            }
        }
    } else if (data_mode) {
        // We were in data mode, but connection dropped
        data_mode = false;
        modem_print("\r\nNO CARRIER\r\n");
    }
}
