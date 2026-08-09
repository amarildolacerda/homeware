# onoff-lora-avr Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a relay ON/OFF node for Arduino Nano that communicates with the existing hub via LoRa 868MHz using a Seeed Grove LoRa module with custom raw SPI firmware.

**Architecture:** Arduino Nano controls relay/button/LED and communicates with the Seeed Grove LoRa module via SoftwareSerial. The module's ATMega168 runs custom firmware that bridges UART to SX1276 SPI in raw mode. The node uses the same `lora_frame_t` protocol as the hub, enabling plug-and-play integration.

**Tech Stack:** Arduino C++, ATmega328P (Arduino Nano), ATmega168 (Seeed module), SX1276 LoRa, SoftwareSerial, SPI, EEPROM.

## Global Constraints

- No `shared/` dependency — AVR code is self-contained
- No ArduinoJson — use packed structs only (2KB RAM limit)
- No WiFi, no dashboard, no OTA — serial console only
- `lora_frame_t` must match `shared/src/lora_protocol.h` exactly
- EEPROM offset >=200 for relay state (per project rule)
- Non-blocking loop — no `delay()` except in setup/serial wait
- Device ID: `avr_<3-byte-signature>` (unique per ATmega328P chip)
- LoRa params: 868MHz, SF10, BW125kHz, CR 4/7, Preamble 8, TX 17dBm

---

## File Map

| File | Responsibility |
|------|---------------|
| `nodes/onoff-lora-avr/include/lora_avr.h` | Protocol structs, message types, constants |
| `nodes/onoff-lora-avr/include/config.h` | Pin definitions, intervals, device config |
| `nodes/onoff-lora-avr/src/lora_avr.cpp` | LoRa communication via SoftwareSerial |
| `nodes/onoff-lora-avr/src/main.cpp` | Setup, loop, relay, button, LED, console |
| `nodes/onoff-lora-avr/firmware/lora_bridge/lora_bridge.ino` | ATMega168 raw SPI bridge firmware |
| `nodes/onoff-lora-avr/platformio.ini` | Build configuration |
| `nodes/onoff-lora-avr/build.sh` | Build script |
| `nodes/onoff-lora-avr/flash.sh` | Flash script |
| `nodes/onoff-lora-avr/monitor.sh` | Monitor script |

---

### Task 1: Protocol Header — `lora_avr.h`

**Files:**
- Create: `nodes/onoff-lora-avr/include/lora_avr.h`

**Interfaces:**
- Produces: `lora_frame_t`, `lora_command_t`, `lora_pair_request_t`, `lora_pair_response_t`, message type enums, `LORA_HEADER_SIZE`, `LORA_MAX_PAYLOAD`

- [ ] **Step 1: Create the protocol header**

```cpp
// nodes/onoff-lora-avr/include/lora_avr.h
#pragma once

#include <stdint.h>
#include <string.h>

// ── Message Types (must match shared/src/msg_type.h) ──
enum msg_type_t : uint8_t {
    MSG_SENSOR_DATA     = 0x01,
    MSG_PAIR_REQUEST    = 0x02,
    MSG_PAIR_RESPONSE   = 0x03,
    MSG_ACK             = 0x04,
    MSG_HEARTBEAT       = 0x05,
    MSG_COMMAND         = 0x07,
    MSG_NAK             = 0x0D,
};

// ── Sensor Types ──
#define SENSOR_TYPE_ONOFF 8

// ── Protocol Structs (packed, match shared/src/lora_protocol.h) ──
#pragma pack(push, 1)

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  payload[];
} lora_frame_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  assigned_slot;
} lora_pair_response_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  sensor_type;
    char     device_name[16];
} lora_pair_request_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  command;
} lora_command_t;

#pragma pack(pop)

#define LORA_HEADER_SIZE   11
#define LORA_MAX_PAYLOAD   200

// ── UART Protocol for ATMega168 Bridge ──
// TX: 'T' + len_hi + len_lo + [data...]
// RX enable: 'R'
// Status: '?' → 'T'(ready), 'B'(busy)
// Response: 'D' + len_hi + len_lo + [data...] (received packet)
//           'T' (TX complete)
//           'E' (error)

#define BRIDGE_CMD_TX      'T'
#define BRIDGE_CMD_RX_EN   'R'
#define BRIDGE_CMD_STATUS  '?'
#define BRIDGE_RSP_TX_OK   'T'
#define BRIDGE_RSP_DATA    'D'
#define BRIDGE_RSP_ERROR   'E'
#define BRIDGE_RSP_READY   'T'
#define BRIDGE_RSP_BUSY    'B'
```

- [ ] **Step 2: Verify header compiles**

Run: `cd nodes/onoff-lora-avr && mkdir -p .pio && echo '#include "lora_avr.h"' > /tmp/test_lora.cpp && g++ -fsyntax-only -std=gnu++17 -I include /tmp/test_lora.cpp 2>&1 || echo "Note: AVR headers need avr-gcc, syntax check skipped"`

Expected: May show warnings about AVR-specific syntax. Struct sizes can be verified on target.

- [ ] **Step 3: Commit**

```bash
git add nodes/onoff-lora-avr/include/lora_avr.h
git commit -m "feat(onoff-lora-avr): add LoRa protocol header with lora_frame_t"
```

---

### Task 2: ATMega168 Firmware — `lora_bridge.ino`

**Files:**
- Create: `nodes/onoff-lora-avr/firmware/lora_bridge/lora_bridge.ino`

**Interfaces:**
- Consumes: None (standalone firmware for ATMega168)
- Produces: UART bridge protocol (T/R/? commands → SX1276 SPI)

- [ ] **Step 1: Create the ATMega168 bridge firmware**

```cpp
// nodes/onoff-lora-avr/firmware/lora_bridge/lora_bridge.ino
// Firmware for ATMega168 on Seeed Grove LoRa module
// Bridges UART (from Arduino Nano) to SX1276 SPI in raw mode.
// No external libraries — direct SPI register access.

#include <SPI.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ── SX1276 Registers (LoRa mode) ──
#define REG_FIFO          0x00
#define REG_OP_MODE       0x01
#define REG_FRF_MSB       0x06
#define REG_FRF_MID       0x07
#define REG_FRF_LSB       0x08
#define REG_PA_CONFIG     0x09
#define REG_OCP           0x0B
#define REG_LNA           0x0C
#define REG_FIFO_ADDR_PTR      0x0D
#define REG_FIFO_TX_BASE_ADDR  0x0E
#define REG_FIFO_RX_BASE_ADDR  0x0F
#define REG_FIFO_RX_CUR_ADDR   0x10
#define REG_IRQ_FLAGS_MASK     0x11
#define REG_IRQ_FLAGS          0x12
#define REG_RX_NB_BYTES        0x13
#define REG_PKT_SNR_VALUE      0x19
#define REG_PKT_RSSI_VALUE     0x1A
#define REG_MODEM_CONFIG_1     0x1D
#define REG_MODEM_CONFIG_2     0x1E
#define REG_SYMB_TIMEOUT_LSB   0x1F
#define REG_PREAMBLE_MSB       0x20
#define REG_PREAMBLE_LSB       0x21
#define REG_PAYLOAD_LENGTH     0x22
#define REG_MAX_PAYLOAD_LENGTH 0x23
#define REG_MODEM_CONFIG_3     0x26
#define REG_SYNC_WORD          0x39
#define REG_INVERTIQ           0x33
#define REG_IMAGE_CAL          0x3B
#define REG_TEMP               0x3C
#define REG_DIO_MAPPING_1      0x40
#define REG_VERSION            0x42
#define REG_PA_DAC             0x4D

// LoRa mode bits
#define LORA_MODE         0x80
#define SLEEP_MODE        0x00
#define STANDBY_MODE      0x01
#define FSTX_MODE         0x02
#define TX_MODE           0x03
#define FSRX_MODE         0x04
#define RX_CONTINUOUS_MODE 0x05
#define RX_SINGLE_MODE    0x06

// IRQ flags
#define IRQ_RX_DONE       0x40
#define IRQ_TX_DONE       0x08
#define IRQ_CAD_DONE      0x04
#define IRQ_CRC_ERROR     0x20

// ── Pin Definitions (Seeed Grove LoRa v1.0) ──
#define SX1276_CS   10  // PB2
#define SX1276_DIO0 2   // PD2 (INT0)
#define SX1276_DIO1 3   // PD3 (INT1)
#define SX1276_RST  A1  // PC1

// ── LoRa Configuration (must match hub) ──
#define LORA_FREQ_HZ    868000000UL
#define LORA_SF         10
#define LORA_BW         125000UL
#define LORA_CR         7       // 4/7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17
#define LORA_SYNC_WORD  0x12

// ── UART ──
#define UART_BAUD 9600

// ── State ──
static volatile bool s_dio0_fired = false;
static uint8_t s_rx_buf[255];
static uint8_t s_rx_len = 0;
static bool s_rx_ready = false;

// ── SX1276 SPI Helpers ──
static void sx_write_reg(uint8_t reg, uint8_t val) {
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(reg | 0x80);
    SPI.transfer(val);
    digitalWrite(SX1276_CS, HIGH);
}

static uint8_t sx_read_reg(uint8_t reg) {
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(reg & 0x7F);
    uint8_t val = SPI.transfer(0x00);
    digitalWrite(SX1276_CS, HIGH);
    return val;
}

static void sx_write_fifo(const uint8_t *data, uint8_t len) {
    sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    sx_write_reg(REG_PAYLOAD_LENGTH, len);
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) SPI.transfer(data[i]);
    digitalWrite(SX1276_CS, HIGH);
}

static uint8_t sx_read_fifo(uint8_t *data, uint8_t max_len) {
    uint8_t len = sx_read_reg(REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    sx_write_reg(REG_FIFO_RX_CUR_ADDR, sx_read_reg(REG_FIFO_RX_CUR_ADDR));
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(REG_FIFO & 0x7F);
    for (uint8_t i = 0; i < len; i++) data[i] = SPI.transfer(0x00);
    digitalWrite(SX1276_CS, HIGH);
    return len;
}

// ── SX1276 Configuration ──
static void sx_set_mode(uint8_t mode) {
    sx_write_reg(REG_OP_MODE, LORA_MODE | mode);
}

static void sx_configure() {
    // Reset
    pinMode(SX1276_RST, OUTPUT);
    digitalWrite(SX1276_RST, LOW);
    delay(10);
    digitalWrite(SX1276_RST, HIGH);
    delay(10);

    // Verify version
    uint8_t ver = sx_read_reg(REG_VERSION);
    if (ver != 0x12) {
        // SX1276 version should be 0x12
        while (1); // Halt on error
    }

    // Set LoRa mode + sleep
    sx_set_mode(SLEEP_MODE);
    delay(5);

    // Frequency: 868 MHz
    // FRF = freq * 2^19 / 32MHz
    uint32_t frf = (uint32_t)((double)LORA_FREQ_HZ * 524288.0 / 32000000.0);
    sx_write_reg(REG_FRF_MSB, (frf >> 16) & 0xFF);
    sx_write_reg(REG_FRF_MID, (frf >> 8) & 0xFF);
    sx_write_reg(REG_FRF_LSB, frf & 0xFF);

    // Modem config: BW=125kHz, CR=4/7, explicit header
    uint8_t mc1 = 0; // BW=125kHz (0111), CR=4/7 (11), implicit header off (0)
    mc1 |= (0x70);   // BW bits [7:4] = 0111 → 125 kHz
    mc1 |= (0x06);   // CR bits [3:1] = 110 → 4/7 (CR=7 means 4/7)
    mc1 |= (0x00);   // Implicit header off
    sx_write_reg(REG_MODEM_CONFIG_1, mc1);

    // Modem config 2: SF=10, TX mode normal
    uint8_t mc2 = 0;
    mc2 |= ((LORA_SF & 0x0F) << 4);  // SF bits [7:4]
    mc2 |= (0x00);   // RXPayloadCrcOn=off (we handle CRC in app)
    mc2 |= (0x00);   // SymbTimeout MSB
    sx_write_reg(REG_MODEM_CONFIG_2, mc2);

    // Modem config 3: low data rate optimize off, AGC auto on
    sx_write_reg(REG_MODEM_CONFIG_3, 0x04);

    // Preamble length
    sx_write_reg(REG_PREAMBLE_MSB, 0x00);
    sx_write_reg(REG_PREAMBLE_LSB, LORA_PREAMBLE);

    // Sync word
    sx_write_reg(REG_SYNC_WORD, LORA_SYNC_WORD);

    // PA config: PA_BOOST, max power
    sx_write_reg(REG_PA_CONFIG, 0x8F); // PA_BOOST, max power

    // OCP
    sx_write_reg(REG_OCP, 0x2B); // 100mA

    // PA DAC for +20dBm
    sx_write_reg(REG_PA_DAC, 0x87);

    // LNA: max gain
    sx_write_reg(REG_LNA, 0x23);

    // DIO0 = TxDone or RxDone
    sx_write_reg(REG_DIO_MAPPING_1, 0x00); // DIO0=RxDone, DIO1=RxTimeout

    // FIFO base addresses
    sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
    sx_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);

    // Clear IRQ flags
    sx_write_reg(REG_IRQ_FLAGS, 0xFF);

    // Standby
    sx_set_mode(STANDBY_MODE);
}

// ── DIO0 Interrupt ──
void dio0_isr() {
    s_dio0_fired = true;
}

// ── UART Helpers ──
static void uart_send_byte(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = b;
}

static void uart_send_buf(const uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) uart_send_byte(buf[i]);
}

static int uart_available() {
    return (UCSR0A & (1 << RXC0)) ? 1 : 0;
}

static uint8_t uart_read() {
    return UDR0;
}

// ── TX Packet ──
static bool sx_tx_packet(const uint8_t *data, uint8_t len) {
    sx_set_mode(STANDBY_MODE);
    delay(1);
    sx_write_fifo(data, len);
    sx_write_reg(REG_IRQ_FLAGS, 0xFF); // Clear all flags
    sx_set_mode(TX_MODE);
    // Wait for DIO0 (TxDone) or timeout
    unsigned long start = millis();
    while (!s_dio0_fired) {
        if (millis() - start > 3000) return false; // 3s timeout
    }
    s_dio0_fired = false;
    sx_set_mode(STANDBY_MODE);
    return true;
}

// ── RX Packet (non-blocking check) ──
static bool sx_rx_check(uint8_t *data, uint8_t *len, int8_t *rssi) {
    if (!s_dio0_fired) return false;
    s_dio0_fired = false;

    uint8_t irq = sx_read_reg(REG_IRQ_FLAGS);
    sx_write_reg(REG_IRQ_FLAGS, 0xFF); // Clear all

    if (irq & IRQ_RX_DONE) {
        *len = sx_read_fifo(data, 255);
        *rssi = sx_read_reg(REG_PKT_RSSI_VALUE) - 157; // SX1276 RSSI offset
        sx_set_mode(STANDBY_MODE);
        return true;
    }
    return false;
}

// ── Setup ──
void setup() {
    // UART to Arduino Nano
    Serial.begin(UART_BAUD);

    // SPI to SX1276
    pinMode(SX1276_CS, OUTPUT);
    digitalWrite(SX1276_CS, HIGH);
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 4MHz SPI
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    // DIO0 interrupt
    pinMode(SX1276_DIO0, INPUT);
    attachInterrupt(digitalPinToInterrupt(SX1276_DIO0), dio0_isr, RISING);

    // Configure SX1276
    sx_configure();
}

// ── Loop ──
void loop() {
    // ── Check for UART commands from Nano ──
    if (uart_available()) {
        uint8_t cmd = uart_read();

        switch (cmd) {
            case BRIDGE_CMD_TX: {
                // Wait for length bytes
                while (!uart_available()) { if (millis() % 100 == 0) yield(); }
                uint8_t len_hi = uart_read();
                while (!uart_available()) { if (millis() % 100 == 0) yield(); }
                uint8_t len_lo = uart_read();
                uint16_t len = ((uint16_t)len_hi << 8) | len_lo;
                if (len > 255) len = 255;

                // Read data
                uint8_t buf[255];
                for (uint16_t i = 0; i < len; i++) {
                    while (!uart_available()) { if (millis() % 100 == 0) yield(); }
                    buf[i] = uart_read();
                }

                // TX
                bool ok = sx_tx_packet(buf, len);
                uart_send_byte(ok ? BRIDGE_RSP_TX_OK : BRIDGE_RSP_ERROR);
                break;
            }

            case BRIDGE_CMD_RX_EN: {
                // Enter RX continuous mode
                sx_set_mode(STANDBY_MODE);
                sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
                sx_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
                sx_write_reg(REG_IRQ_FLAGS, 0xFF);
                sx_set_mode(RX_CONTINUOUS_MODE);
                s_rx_ready = true;
                uart_send_byte(BRIDGE_RSP_READY);
                break;
            }

            case BRIDGE_CMD_STATUS: {
                uart_send_byte(s_rx_ready ? BRIDGE_RSP_READY : BRIDGE_RSP_BUSY);
                break;
            }
        }
    }

    // ── Check for received LoRa packet ──
    if (s_rx_ready) {
        uint8_t buf[255];
        uint8_t len = 0;
        int8_t rssi = 0;

        if (sx_rx_check(buf, &len, &rssi)) {
            // Send to Nano: 'D' + len_hi + len_lo + [data...]
            uart_send_byte(BRIDGE_RSP_DATA);
            uart_send_byte((len >> 8) & 0xFF);
            uart_send_byte(len & 0xFF);
            uart_send_buf(buf, len);

            // Re-enter RX mode
            sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
            sx_write_reg(REG_IRQ_FLAGS, 0xFF);
            sx_set_mode(RX_CONTINUOUS_MODE);
        }
    }
}
```

- [ ] **Step 2: Verify firmware compiles**

Run: `cd nodes/onoff-lora-avr/firmware/lora_bridge && arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 . 2>&1 || echo "Note: requires arduino-cli with AVR core installed"`

Expected: Compilation succeeds (may need arduino-cli + avr core installed)

- [ ] **Step 3: Commit**

```bash
git add nodes/onoff-lora-avr/firmware/lora_bridge/lora_bridge.ino
git commit -m "feat(onoff-lora-avr): ATMega168 raw SPI bridge firmware"
```

---

### Task 3: Config Header — `config.h`

**Files:**
- Create: `nodes/onoff-lora-avr/include/config.h`

**Interfaces:**
- Produces: Pin definitions, timing constants, device name, EEPROM offsets

- [ ] **Step 1: Create config header**

```cpp
// nodes/onoff-lora-avr/include/config.h
#pragma once

// ── Device ──
#ifndef DEVICE_NAME
#define DEVICE_NAME "LoRa AVR Switch"
#endif

// ── Pins ──
#ifndef LORA_RX_PIN
#define LORA_RX_PIN 2
#endif
#ifndef LORA_TX_PIN
#define LORA_TX_PIN 3
#endif
#ifndef LED_PIN
#define LED_PIN 4
#endif
#ifndef RELAY_PIN
#define RELAY_PIN 5
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN 6
#endif

// ── Relay ──
#ifndef RELAY_ON
#define RELAY_ON HIGH
#endif

// ── Serial ──
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
#ifndef LORA_BAUD
#define LORA_BAUD 9600
#endif

// ── LoRa ──
#ifndef LORA_FREQ
#define LORA_FREQ 868.0
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_BW
#define LORA_BW 125
#endif
#ifndef LORA_CR
#define LORA_CR 7
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 17
#endif

// ── Timing ──
#ifndef PAIR_INTERVAL_MS
#define PAIR_INTERVAL_MS 5000
#endif
#ifndef PAIR_MAX_ATTEMPTS
#define PAIR_MAX_ATTEMPTS 20
#endif
#ifndef STATE_UPDATE_INTERVAL_MS
#define STATE_UPDATE_INTERVAL_MS 60000
#endif
#ifndef HEARTBEAT_INTERVAL_MS
#define HEARTBEAT_INTERVAL_MS 60000
#endif
#ifndef LED_BLINK_PAIR_MS
#define LED_BLINK_PAIR_MS 250
#endif
#ifndef LED_BLINK_BOOT_MS
#define LED_BLINK_BOOT_MS 500
#endif
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 50
#endif

// ── EEPROM ──
#define EEPROM_SIZE 512
#define EEPROM_RELAY_STATE 200
#define EEPROM_PAIRED_FLAG 201
#define EEPROM_MY_MAC      202  // 6 bytes (202-207)
```

- [ ] **Step 2: Commit**

```bash
git add nodes/onoff-lora-avr/include/config.h
git commit -m "feat(onoff-lora-avr): config header with pins and timing"
```

---

### Task 4: LoRa Communication Module — `lora_avr.cpp` + `lora_avr.h`

**Files:**
- Create: `nodes/onoff-lora-avr/src/lora_avr.h` (internal header)
- Create: `nodes/onoff-lora-avr/src/lora_avr.cpp`

**Interfaces:**
- Consumes: `lora_avr.h` (protocol), `config.h` (pins/baud)
- Produces: `lora_init()`, `lora_send_frame()`, `lora_send_pair_request()`, `lora_send_sensor_data()`, `lora_send_heartbeat()`, `lora_loop()`, `lora_is_paired()`, `lora_get_slot()`, `lora_get_last_rssi()`, `lora_rx_count()`, `lora_tx_count()`

- [ ] **Step 1: Create internal LoRa header**

```cpp
// nodes/onoff-lora-avr/src/lora_avr.h
#pragma once

#include <Arduino.h>
#include "../include/lora_avr.h"
#include "../include/config.h"

// Initialize SoftwareSerial and bridge
void lora_init(const uint8_t *my_mac, const char *device_name);

// Send a raw frame
bool lora_send_frame(const uint8_t *data, uint8_t len);

// Convenience senders
bool lora_send_pair_request(uint8_t sensor_type);
bool lora_send_sensor_data(uint8_t relay_state);
bool lora_send_heartbeat();

// Non-blocking loop: check for received commands
// Calls on_command(slot, command) when a MSG_COMMAND is received
typedef void (*lora_command_callback_t)(uint8_t slot, uint8_t command);
void lora_set_command_callback(lora_command_callback_t cb);

void lora_loop();

// State
bool lora_is_paired();
uint8_t lora_get_slot();
int8_t lora_get_last_rssi();
uint32_t lora_rx_count();
uint32_t lora_tx_count();
```

- [ ] **Step 2: Create LoRa communication implementation**

```cpp
// nodes/onoff-lora-avr/src/lora_avr.cpp
#include "lora_avr.h"
#include <SoftwareSerial.h>

static SoftwareSerial s_lora_serial(LORA_RX_PIN, LORA_TX_PIN);

static uint8_t s_my_mac[6];
static char s_device_name[16];
static uint16_t s_sequence = 0;
static uint8_t s_slot = 0;
static bool s_paired = false;
static int8_t s_last_rssi = 0;
static uint32_t s_rx_count = 0;
static uint32_t s_tx_count = 0;

static lora_command_callback_t s_command_cb = nullptr;

// ── Bridge UART Protocol ──

static void bridge_send_tx(const uint8_t *data, uint8_t len) {
    s_lora_serial.write(BRIDGE_CMD_TX);
    s_lora_serial.write((len >> 8) & 0xFF);
    s_lora_serial.write(len & 0xFF);
    s_lora_serial.write(data, len);
}

static void bridge_send_rx_enable() {
    s_lora_serial.write(BRIDGE_CMD_RX_EN);
}

static void bridge_send_status() {
    s_lora_serial.write(BRIDGE_CMD_STATUS);
}

// ── Frame Builders ──

static uint8_t build_frame(uint8_t msg_type, uint8_t *buf, uint8_t max_len,
                           const uint8_t *payload, uint8_t payload_len) {
    if (max_len < LORA_HEADER_SIZE + payload_len) return 0;

    lora_frame_t *f = (lora_frame_t *)buf;
    f->msg_type = msg_type;
    f->sequence = s_sequence++;
    memcpy(f->sensor_id, s_my_mac, 6);
    f->rssi = 0;
    f->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(f->payload, payload, payload_len);
    }
    return LORA_HEADER_SIZE + payload_len;
}

// ── Public Functions ──

void lora_init(const uint8_t *my_mac, const char *device_name) {
    memcpy(s_my_mac, my_mac, 6);
    strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
    s_device_name[sizeof(s_device_name) - 1] = '\0';

    s_lora_serial.begin(LORA_BAUD);
    delay(100);

    // Enable RX on bridge
    bridge_send_rx_enable();
    delay(50);
}

bool lora_send_frame(const uint8_t *data, uint8_t len) {
    // Wait for bridge ready
    unsigned long start = millis();
    bridge_send_status();
    while (millis() - start < 100) {
        if (s_lora_serial.available()) {
            uint8_t rsp = s_lora_serial.read();
            if (rsp == BRIDGE_RSP_READY) break;
            if (rsp == BRIDGE_RSP_BUSY) {
                delay(10);
                bridge_send_status();
            }
        }
    }

    bridge_send_tx(data, len);

    // Wait for TX OK or error
    start = millis();
    while (millis() - start < 3000) {
        if (s_lora_serial.available()) {
            uint8_t rsp = s_lora_serial.read();
            if (rsp == BRIDGE_RSP_TX_OK) {
                s_tx_count++;
                return true;
            }
            if (rsp == BRIDGE_RSP_ERROR) return false;
        }
    }
    return false;
}

bool lora_send_pair_request(uint8_t sensor_type) {
    uint8_t payload[1 + 16];
    payload[0] = sensor_type;
    memset(payload + 1, 0, 16);
    strncpy((char *)(payload + 1), s_device_name, 15);

    uint8_t buf[LORA_HEADER_SIZE + 17];
    uint8_t len = build_frame(MSG_PAIR_REQUEST, buf, sizeof(buf), payload, 17);
    return lora_send_frame(buf, len);
}

bool lora_send_sensor_data(uint8_t relay_state) {
    uint8_t buf[LORA_HEADER_SIZE + 1];
    uint8_t len = build_frame(MSG_SENSOR_DATA, buf, sizeof(buf), &relay_state, 1);
    return lora_send_frame(buf, len);
}

bool lora_send_heartbeat() {
    uint8_t buf[LORA_HEADER_SIZE];
    uint8_t len = build_frame(MSG_HEARTBEAT, buf, sizeof(buf), nullptr, 0);
    return lora_send_frame(buf, len);
}

void lora_set_command_callback(lora_command_callback_t cb) {
    s_command_cb = cb;
}

void lora_loop() {
    while (s_lora_serial.available()) {
        uint8_t rsp = s_lora_serial.read();

        if (rsp == BRIDGE_RSP_DATA) {
            // Wait for length
            while (!s_lora_serial.available());
            uint8_t len_hi = s_lora_serial.read();
            while (!s_lora_serial.available());
            uint8_t len_lo = s_lora_serial.read();
            uint16_t len = ((uint16_t)len_hi << 8) | len_lo;
            if (len > 255) len = 255;

            // Read data
            uint8_t buf[255];
            uint16_t idx = 0;
            unsigned long timeout = millis();
            while (idx < len && millis() - timeout < 1000) {
                if (s_lora_serial.available()) {
                    buf[idx++] = s_lora_serial.read();
                    timeout = millis();
                }
            }

            if (idx < LORA_HEADER_SIZE) return;

            const lora_frame_t *frame = (const lora_frame_t *)buf;
            s_last_rssi = frame->rssi;
            s_rx_count++;

            // Check if addressed to us
            if (memcmp(frame->sensor_id, s_my_mac, 6) != 0) return;

            switch (frame->msg_type) {
                case MSG_PAIR_RESPONSE: {
                    if (frame->payload_len >= 1) {
                        const lora_pair_response_t *resp = (const lora_pair_response_t *)buf;
                        s_slot = resp->assigned_slot;
                        s_paired = true;
                    }
                    break;
                }

                case MSG_COMMAND: {
                    if (frame->payload_len >= 1 && s_command_cb) {
                        const lora_command_t *cmd = (const lora_command_t *)buf;
                        s_command_cb(s_slot, cmd->command);
                    }
                    break;
                }

                case MSG_NAK: {
                    // Log and ignore for now
                    break;
                }
            }
        }
    }
}

bool lora_is_paired() { return s_paired; }
uint8_t lora_get_slot() { return s_slot; }
int8_t lora_get_last_rssi() { return s_last_rssi; }
uint32_t lora_rx_count() { return s_rx_count; }
uint32_t lora_tx_count() { return s_tx_count; }
```

- [ ] **Step 3: Commit**

```bash
git add nodes/onoff-lora-avr/src/lora_avr.h nodes/onoff-lora-avr/src/lora_avr.cpp
git commit -m "feat(onoff-lora-avr): LoRa communication module via SoftwareSerial"
```

---

### Task 5: Main Application — `main.cpp`

**Files:**
- Create: `nodes/onoff-lora-avr/src/main.cpp`

**Interfaces:**
- Consumes: `lora_avr.h` (init, send, loop, state), `config.h` (pins, timing)
- Produces: Complete node with relay, button, LED, serial console

- [ ] **Step 1: Create main application**

```cpp
// nodes/onoff-lora-avr/src/main.cpp
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include "../include/config.h"
#include "../include/lora_avr.h"
#include "lora_avr.h"

// ── State ──
static bool s_relay = false;
static bool s_paired = false;
static uint8_t s_my_mac[6];
static char s_device_id[16];
static char s_device_name[32] = DEVICE_NAME;

// ── Timing ──
static unsigned long s_last_state_send = 0;
static unsigned long s_last_heartbeat = 0;
static unsigned long s_last_pair_attempt = 0;
static uint8_t s_pair_attempts = 0;
static unsigned long s_last_led_toggle = 0;
static bool s_led_state = false;

// ── EEPROM ──

static void relay_save() {
    EEPROM.write(EEPROM_RELAY_STATE, s_relay ? 1 : 0);
}

static void relay_load() {
    s_relay = (EEPROM.read(EEPROM_RELAY_STATE) == 1);
}

static void paired_save() {
    EEPROM.write(EEPROM_PAIRED_FLAG, 1);
}

static void paired_load() {
    s_paired = (EEPROM.read(EEPROM_PAIRED_FLAG) == 1);
}

static void mac_save() {
    for (uint8_t i = 0; i < 6; i++) {
        EEPROM.write(EEPROM_MY_MAC + i, s_my_mac[i]);
    }
}

static void mac_load() {
    for (uint8_t i = 0; i < 6; i++) {
        s_my_mac[i] = EEPROM.read(EEPROM_MY_MAC + i);
    }
}

// ── Device ID from ATmega328P signature ──

static void generate_device_id() {
    // Read signature bytes from boot section
    uint8_t sig0 = pgm_read_byte(0x00);
    uint8_t sig1 = pgm_read_byte(0x01);
    uint8_t sig2 = pgm_read_byte(0x02);

    s_my_mac[0] = sig0;
    s_my_mac[1] = sig1;
    s_my_mac[2] = sig2;
    s_my_mac[3] = 0x00;
    s_my_mac[4] = 0x00;
    s_my_mac[5] = 0x00;

    snprintf(s_device_id, sizeof(s_device_id), "avr_%02X%02X%02X", sig0, sig1, sig2);
}

// ── Relay ──

static void set_relay(bool state) {
    s_relay = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : !RELAY_ON);
    relay_save();
    s_last_state_send = 0; // Force immediate send
    Serial.print("Relay: ");
    Serial.println(state ? "ON" : "OFF");
}

static void toggle_relay() {
    set_relay(!s_relay);
}

// ── LoRa Command Callback ──

static void on_lora_command(uint8_t slot, uint8_t command) {
    if (command == 0x01) {
        set_relay(true);
    } else if (command == 0x00) {
        set_relay(false);
    } else if (command == 0xFF) {
        Serial.println("Restart command received");
        delay(100);
        asm volatile("jmp 0x0000");
    }
}

// ── LED ──

static void led_update() {
    unsigned long now = millis();

    if (!s_paired) {
        // Blink during pairing
        if (now - s_last_led_toggle > LED_BLINK_PAIR_MS) {
            s_last_led_toggle = now;
            s_led_state = !s_led_state;
            digitalWrite(LED_PIN, s_led_state ? LOW : HIGH);
        }
    } else {
        // Solid: ON if relay active, OFF if relay off
        digitalWrite(LED_PIN, s_relay ? LOW : HIGH);
    }
}

// ── Serial Console ──

static void print_help() {
    Serial.println("\n=== Comandos ===");
    Serial.println("  h/?  - Ajuda");
    Serial.println("  s    - Status");
    Serial.println("  l    - Alterna rele");
    Serial.println("  p    - Re-parear");
    Serial.println("  r    - Reiniciar");
    Serial.println("================\n");
}

static void print_status() {
    Serial.println("\n=== Status ===");
    Serial.print("  Device:  "); Serial.println(s_device_id);
    Serial.print("  Nome:    "); Serial.println(s_device_name);
    Serial.print("  Rele:    "); Serial.println(s_relay ? "ON" : "OFF");
    Serial.print("  Pareado: "); Serial.println(s_paired ? "Sim" : "Nao");
    Serial.print("  Slot:    "); Serial.println(s_paired ? "0" : "-");
    Serial.print("  RSSI:    "); Serial.print(s_last_rssi); Serial.println(" dBm (LoRa)");
    Serial.print("  Uptime:  "); Serial.print(millis() / 1000); Serial.println("s");
    Serial.print("  RX/TX:   "); Serial.print(lora_rx_count()); Serial.print(" / "); Serial.println(lora_tx_count());
    Serial.println("=============\n");
}

static void handle_serial(char c) {
    switch (c) {
        case 'h': case 'H': case '?':
            print_help();
            break;
        case 's': case 'S':
            print_status();
            break;
        case 'l': case 'L':
            toggle_relay();
            break;
        case 'p': case 'P':
            s_paired = false;
            s_pair_attempts = 0;
            s_last_pair_attempt = 0;
            paired_save();
            Serial.println("Re-pareando...");
            break;
        case 'r': case 'R':
            Serial.println("Reiniciando...");
            delay(100);
            asm volatile("jmp 0x0000");
            break;
    }
}

// ── Setup ──

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    Serial.println("\n\nLoRa AVR Switch starting...");

    // Generate device ID
    generate_device_id();
    Serial.print("Device: ");
    Serial.println(s_device_id);

    // Init hardware
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Load state from EEPROM
    relay_load();
    paired_load();
    mac_load();

    // Check if MAC is valid (all zeros = first boot)
    bool mac_valid = false;
    for (uint8_t i = 0; i < 6; i++) {
        if (s_my_mac[i] != 0x00) { mac_valid = true; break; }
    }
    if (!mac_valid) {
        memcpy(s_my_mac, s_my_mac, 6); // Already set by generate_device_id
        mac_save();
    }

    digitalWrite(RELAY_PIN, s_relay ? RELAY_ON : !RELAY_ON);
    Serial.print("Relay: ");
    Serial.println(s_relay ? "ON" : "OFF");

    // Init LoRa
    lora_init(s_my_mac, s_device_name);
    lora_set_command_callback(on_lora_command);
    Serial.println("LoRa initialized");

    Serial.println("Ready! 'h' for help\n");
}

// ── Loop ──

void loop() {
    unsigned long now = millis();

    // Serial console
    if (Serial.available()) {
        handle_serial(Serial.read());
    }

    // LoRa communication
    lora_loop();

    // Pairing
    if (!s_paired) {
        if (s_pair_attempts < PAIR_MAX_ATTEMPTS &&
            now - s_last_pair_attempt >= PAIR_INTERVAL_MS) {
            s_last_pair_attempt = now;
            s_pair_attempts++;
            Serial.print("Pair attempt ");
            Serial.print(s_pair_attempts);
            Serial.print("/");
            Serial.println(PAIR_MAX_ATTEMPTS);
            lora_send_pair_request(SENSOR_TYPE_ONOFF);
        }
    } else {
        // Send state periodically
        if (now - s_last_state_send >= STATE_UPDATE_INTERVAL_MS) {
            s_last_state_send = now;
            lora_send_sensor_data(s_relay ? 1 : 0);
        }

        // Heartbeat
        if (now - s_last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            s_last_heartbeat = now;
            lora_send_heartbeat();
        }
    }

    // Button
    static unsigned long s_last_button_check = 0;
    if (now - s_last_button_check > BUTTON_DEBOUNCE_MS) {
        s_last_button_check = now;
        if (digitalRead(BUTTON_PIN) == LOW) {
            delay(BUTTON_DEBOUNCE_MS);
            if (digitalRead(BUTTON_PIN) == LOW) {
                while (digitalRead(BUTTON_PIN) == LOW) delay(10);
                toggle_relay();
            }
        }
    }

    // LED
    led_update();

    delay(1);
}
```

- [ ] **Step 2: Commit**

```bash
git add nodes/onoff-lora-avr/src/main.cpp
git commit -m "feat(onoff-lora-avr): main application with relay, button, console"
```

---

### Task 6: platformio.ini and Build Scripts

**Files:**
- Create: `nodes/onoff-lora-avr/platformio.ini`
- Create: `nodes/onoff-lora-avr/build.sh`
- Create: `nodes/onoff-lora-avr/flash.sh`
- Create: `nodes/onoff-lora-avr/monitor.sh`

**Interfaces:**
- Consumes: All source files from previous tasks
- Produces: Buildable project

- [ ] **Step 1: Create platformio.ini**

```ini
; nodes/onoff-lora-avr/platformio.ini
[env:nano_lora]
platform = atmelavr
board = nanoatmega328
framework = arduino
monitor_speed = 115200
build_flags =
    -DLORA_RX_PIN=2
    -DLORA_TX_PIN=3
    -DRELAY_PIN=5
    -DBUTTON_PIN=6
    -DLED_PIN=4
    -DLORA_FREQ=868.0
    -DLORA_SF=10
    -DLORA_BW=125
    -DLORA_CR=7
    -DLORA_TX_POWER=17
    -DDEVICE_NAME=\"LoRa AVR Switch\"
```

- [ ] **Step 2: Create build.sh**

```bash
#!/bin/bash
# nodes/onoff-lora-avr/build.sh
set -e
cd "$(dirname "$0")"
pio run
```

- [ ] **Step 3: Create flash.sh**

```bash
#!/bin/bash
# nodes/onoff-lora-avr/flash.sh
set -e
cd "$(dirname "$0")"
PORT="${1:-/dev/ttyUSB0}"
pio run --target upload --upload-port "$PORT"
```

- [ ] **Step 4: Create monitor.sh**

```bash
#!/bin/bash
# nodes/onoff-lora-avr/monitor.sh
set -e
cd "$(dirname "$0")"
PORT="${1:-/dev/ttyUSB0}"
pio device monitor --port "$PORT"
```

- [ ] **Step 5: Make scripts executable**

```bash
chmod +x nodes/onoff-lora-avr/build.sh nodes/onoff-lora-avr/flash.sh nodes/onoff-lora-avr/monitor.sh
```

- [ ] **Step 6: Commit**

```bash
git add nodes/onoff-lora-avr/platformio.ini nodes/onoff-lora-avr/build.sh nodes/onoff-lora-avr/flash.sh nodes/onoff-lora-avr/monitor.sh
git commit -m "feat(onoff-lora-avr): platformio config and build scripts"
```

---

## Implementation Order

```
Task 1 (lora_avr.h)     ← Protocol structs, no deps
Task 2 (lora_bridge.ino) ← ATMega168 firmware, standalone
Task 3 (config.h)        ← Config constants, no deps
Task 4 (lora_avr.cpp)    ← Depends on Task 1 + 3
Task 5 (main.cpp)        ← Depends on Task 3 + 4
Task 6 (platformio + scripts) ← Depends on all above
```

Tasks 1, 2, 3 can be done in parallel.
Tasks 4 depends on 1+3. Task 5 depends on 3+4. Task 6 is last.

## Verification

After all tasks:
1. `pio run` in `nodes/onoff-lora-avr/` — should compile for ATmega328P
2. Flash to Arduino Nano — serial output shows device ID, relay state, LoRa init
3. Flash ATMega168 with `lora_bridge.ino` via ISP programmer
4. Connect Nano D2→Seeed TX, D3→Seeed RX
5. Open serial monitor — node attempts pairing every 5s
6. On hub: node appears in sensor registry after pair
7. Toggle relay via serial 'l' — hub receives state update
8. Send command from hub — node toggles relay
