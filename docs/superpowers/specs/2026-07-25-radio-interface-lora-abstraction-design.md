# Radio Interface + LoRa Abstraction Design

Date: 2026-07-25
Status: Draft
FW_VERSION: v0.0.30+

## 1. Objective

Criar uma camada de abstração de rádio padronizada em `shared/` que permita:
- Definir uma interface comum (init/send/recv/loop) para qualquer rádio (LoRa, futuro RFM69, etc.)
- Padronizar o formato de pacote LoRa em uma struct
- Reaproveitar tipos de sensor e payloads já definidos em `espnow_protocol.h`
- O hub herda da interface e implementa o LoRa para TTGO LORA32 com RadioLib

## 2. Arquitetura

```
shared/src/
├── radio_interface.h     → Abstract class RadioInterface
├── lora_protocol.h       → struct lora_frame_t, msg types, pair structs
└── espnow_protocol.h     → (unchanged)

hub/src/
├── lora_handler.h        → class LoraHandler : public RadioInterface
├── lora_handler.cpp      → RadioLib SX1278, TTGO pins, ISR, dispatch
├── display_handler.h/cpp → (unchanged)
└── main.cpp              → #ifdef HABILITA_LORA use LoraHandler

hub/include/
├── lora_config.h         → Pin definitions, freq, SF, BW, CR
└── (other configs)
```

### 2.1 Dependências

```
LoraHandler ──→ RadioInterface (shared)
             ──→ lora_protocol (shared)
             ──→ RadioLib (library)
             ──→ sensor_registry (hub)
             ──→ mqtt_client (hub)
```

## 3. `shared/src/radio_interface.h` — Abstract Interface

```cpp
#ifndef HW_SHARED_RADIO_INTERFACE_H
#define HW_SHARED_RADIO_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

class RadioInterface {
public:
    virtual ~RadioInterface() {}

    // Initialize the radio hardware. Return 0 on success, error code on failure.
    virtual int init() = 0;

    // Send data. Return 0 on success, error code on failure.
    virtual int send(const uint8_t* data, size_t len) = 0;

    // Called from main loop() — handle ISR flags, parse, dispatch.
    virtual void loop() = 0;

    // Returns true if radio is initialized and ready.
    virtual bool is_ready() const = 0;

    // Callback type for received packets.
    using rx_callback_t = void (*)(const uint8_t* data, size_t len,
                                    int16_t rssi, void* arg);

    // Register a callback to be called when a packet is received.
    void set_rx_callback(rx_callback_t cb, void* arg = nullptr) {
        m_rx_cb = cb;
        m_rx_arg = arg;
    }

protected:
    rx_callback_t m_rx_cb = nullptr;
    void* m_rx_arg = nullptr;
};

#endif
```

## 4. `shared/src/lora_protocol.h` — LoRa Protocol Definition

### 4.1 Frame Structure

```
┌──────────┬────────┬──────────────┬──────┬─────────────┬──────────┐
│ msg_type │  seq   │  sensor_id   │ rssi │ payload_len │  payload │
│  1 byte  │ 2 bytes│   6 bytes    │1 byte│   1 byte    │  N bytes │
└──────────┴────────┴──────────────┴──────┴─────────────┴──────────┘
Total header: 11 bytes. Max payload: ~200 bytes (LoRa budget).
```

### 4.2 Packed Structs

```cpp
#ifndef HW_SHARED_LORA_PROTOCOL_H
#define HW_SHARED_LORA_PROTOCOL_H

#include <stdint.h>
#include "espnow_protocol.h"  // for sensor_type_t, payload_*_t

#pragma pack(push, 1)

// Generic LoRa frame header + payload
typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  payload[];       // flexible array (C99)
} lora_frame_t;

// Pair response (hub → node)
typedef struct {
    uint8_t  msg_type;        // LORA_MSG_PAIR_RESPONSE
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;     // 1
    uint8_t  assigned_slot;
} lora_pair_response_t;

// NAK (hub → node)
typedef struct {
    uint8_t  msg_type;        // LORA_MSG_NAK
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;     // 1
    uint8_t  reason;
} lora_nak_t;

// Pair request (node → hub)
typedef struct {
    uint8_t  msg_type;        // LORA_MSG_PAIR_REQUEST
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;     // 1+16
    uint8_t  sensor_type;
    char     device_name[16];
} lora_pair_request_t;

#pragma pack(pop)

// Message types
enum lora_msg_type_t {
    LORA_MSG_SENSOR_DATA   = 0x01,
    LORA_MSG_PAIR_REQUEST  = 0x02,
    LORA_MSG_PAIR_RESPONSE = 0x03,
    LORA_MSG_HEARTBEAT     = 0x04,
    LORA_MSG_NAK           = 0x05,
    LORA_MSG_GW_ANNOUNCE   = 0x06,
    LORA_MSG_COMMAND       = 0x07,
};

// NAK reasons
enum lora_nak_reason_t {
    LORA_NAK_REGISTRY_FULL    = 0x01,
    LORA_NAK_PAIRING_DENIED   = 0x02,
};

// Pair status
#define LORA_PAIR_STATUS_OK     0
#define LORA_PAIR_STATUS_FULL   1
#define LORA_PAIR_STATUS_DENIED 2

#define LORA_HEADER_SIZE        11
#define LORA_FRAME_OVERHEAD     LORA_HEADER_SIZE
#define LORA_MAX_PAYLOAD        200

// Bake-off-size checks
static_assert(sizeof(lora_pair_response_t) == 12, "lora_pair_response_t size mismatch");
static_assert(sizeof(lora_nak_t) == 12, "lora_nak_t size mismatch");

#endif
```

### 4.3 Reuse

- `sensor_type_t` enum → from `espnow_protocol.h`
- `payload_temp_hum_t`, `payload_gas_t`, etc. → from `espnow_protocol.h`
- `mac_to_str()`, `mac_equal()`, `mac_copy()` → from `espnow_protocol.h`

## 5. `hub/include/lora_config.h` — TTGO Pins & Radio Params

```cpp
#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

// TTGO LORA32 T3_v1.6 pin mapping
#define LORA_SS_PIN     18   // NSS/CS
#define LORA_RST_PIN    14   // RST
#define LORA_DIO0_PIN   26   // DIO0 (RX done IRQ)
#define LORA_DIO1_PIN   35   // DIO1 (for CAD, not used in RX)

// Radio parameters
#define LORA_FREQ       868.0  // MHz (EU) / 915.0 (US)
#define LORA_SF         10
#define LORA_BW         125    // kHz
#define LORA_CR         7      // 4/7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17     // dBm

#endif
```

Note: `MCU_TTGO` define removed — pins are unconditional for this env.
GPIO 15 (OLED SCL) vs DIO1 conflict resolved by not using DIO1 in RX.

## 6. `hub/src/lora_handler.h/.cpp` — Concrete Implementation

### Header

```cpp
#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "radio_interface.h"
#include "lora_protocol.h"

class LoraHandler : public RadioInterface {
public:
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
};

#endif
```

### Implementation Outline

- `init()`: RadioLib `SX1276::begin()`, set CRC, set DIO0 ISR, `startReceive()`
- `send()`: `radio.transmit()` (blocking, RadioLib v7)
- `loop()`: check `s_lora_rx_flag` ISR flag → `readData()` → call `m_rx_cb`
- ISR: only sets flag, no I/O
- Parsing uses `lora_frame_t` struct
- Entire `.cpp` guarded by `#ifdef HABILITA_LORA`
- Private members: `Module`, `SX1276`, `s_lora_ok`, `s_lora_rx_flag`, `s_lora_buf`, etc.

### Callback Integration in Hub

The hub's `main.cpp` or `setup()` registers a `rx_callback` on the LoraHandler instance. The callback dispatches to `sensor_registry` and `mqtt_client`, exactly as the current `lora_parse_packet()` does.

## 7. `library.json`

No changes needed. PlatformIO com `lib_extra_dirs = ../shared` já inclui todos os `.h` e `.cpp` de `shared/src/` automaticamente.

## 8. `platformio.ini` Update

```ini
[env:hub_32_lora]
extends = env:hub_32
build_flags =
    ${env:hub_32.build_flags}
    -D HABILITA_LORA
    -D HABILITA_DISPLAY_TTGO
lib_deps =
    ${env:hub_32.lib_deps}
    jgromes/RadioLib@^7.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.9
```

(Already fixed from AP issue — `MCU_TTGO` removed, pins declared unconditionally in `lora_config.h`.)

## 9. Migration Path

| Step | File Change | Scope |
|------|-----------|-------|
| 1 | Create `shared/src/radio_interface.h` | New |
| 2 | Create `shared/src/lora_protocol.h` | New |
| 3 | Update `shared/library.json` | Modified |
| 4 | Rewrite `hub/src/lora_handler.h` | Modified |
| 5 | Rewrite `hub/src/lora_handler.cpp` | Modified |
| 6 | Update `hub/include/lora_config.h` | Modified |
| 7 | Update `hub/src/main.cpp` — use `LoraHandler` class | Modified |
| 8 | Build test `hub_32_lora` | Verify |
| 9 | Update `hub/SPEC_LORA.md` | Modified |

## 10. Coexistence with ESP-NOW

No changes to ESP-NOW handler. The `sensor_registry` receives data from both ESP-NOW (via existing `espnow_handler`) and LoRa (via `LoraHandler::rx_callback`). The registry is transport-agnostic.

## 11. Non-Goals

- No changes to existing ESP-NOW nodes
- No LoRa node implementation (future work)
- No changes to `hub_32` (ESP-NOW only) env
- No changes to display handler abstraction
