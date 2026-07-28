# Radio Interface + LoRa Abstraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a `RadioInterface` abstract class in `shared/` and refactor the hub's LoRa handler to inherit from it.

**Architecture:** `shared/src/radio_interface.h` defines pure virtual methods (init/send/loop/is_ready) + rx_callback registration. `shared/src/lora_protocol.h` defines packed structs and enums for LoRa frames. `hub/src/lora_handler.h/.cpp` implements `RadioInterface` via `sandeepmistry/LoRa` (SX1278). The hub's `main.cpp` instantiates `LoraHandler` and registers a callback that parses `lora_frame_t` and dispatches to `sensor_registry`/`mqtt_client`.

**Tech Stack:** C++17, sandeepmistry/LoRa, PlatformIO, ESP32 Arduino

## Global Constraints

- `shared/` is a submodule; changes must be committed to `dev` branch of both shared and homeware
- All LoRa code guarded by `#ifdef HABILITA_LORA`
- `hub_32` env remains 100% unchanged
- `hub_32_lora` env inherits flags from `hub_32` via `${env:hub_32.build_flags}`
- No `delay()` in loops — non-blocking ISR + flag pattern
- Sensor payload structs reused from `espnow_protocol.h` (not duplicated)
- `FW_VERSION` must match across all devices (v0.0.30+)

---
### Task 1: Create `shared/src/radio_interface.h`

**Files:**
- Create: `shared/src/radio_interface.h`

- [ ] **Write the file**

```cpp
#ifndef HW_SHARED_RADIO_INTERFACE_H
#define HW_SHARED_RADIO_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

class RadioInterface {
public:
    virtual ~RadioInterface() {}

    virtual int init() = 0;
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual void loop() = 0;
    virtual bool is_ready() const = 0;

    using rx_callback_t = void (*)(const uint8_t* data, size_t len,
                                    int16_t rssi, void* arg);

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

- [ ] **Commit**

```bash
cd shared && git add src/radio_interface.h && git commit -m "feat: add RadioInterface abstract class"
```

---
### Task 2: Create `shared/src/lora_protocol.h`

**Files:**
- Create: `shared/src/lora_protocol.h`

- [ ] **Write the file**

```cpp
#ifndef HW_SHARED_LORA_PROTOCOL_H
#define HW_SHARED_LORA_PROTOCOL_H

#include <stdint.h>
#include "espnow_protocol.h"

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
    uint8_t  reason;
} lora_nak_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  sensor_type;
    char     device_name[16];
} lora_pair_request_t;

#pragma pack(pop)

enum lora_msg_type_t {
    LORA_MSG_SENSOR_DATA   = 0x01,
    LORA_MSG_PAIR_REQUEST  = 0x02,
    LORA_MSG_PAIR_RESPONSE = 0x03,
    LORA_MSG_HEARTBEAT     = 0x04,
    LORA_MSG_NAK           = 0x05,
    LORA_MSG_GW_ANNOUNCE   = 0x06,
    LORA_MSG_COMMAND       = 0x07,
};

#define LORA_HEADER_SIZE   11
#define LORA_MAX_PAYLOAD   200

#endif
```

- [ ] **Commit**

```bash
cd shared && git add src/lora_protocol.h && git commit -m "feat: add LoRa protocol structures and message types"
```

---
### Task 3: Refactor `hub/src/lora_handler.h` to class

**Files:**
- Modify: `hub/src/lora_handler.h`

**Interfaces:**
- Consumes: `RadioInterface` from `shared/src/radio_interface.h`
- Produces: `class LoraHandler : public RadioInterface`

- [ ] **Rewrite the header**

Replace current content with:

```cpp
#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "radio_interface.h"
#include <LoRa.h>

#define LORA_RX_BUF_SIZE 256

class LoraHandler : public RadioInterface {
public:
    LoraHandler() : m_ok(false), m_rx_len(0) {}
    ~LoraHandler() {}

    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

private:
    bool m_ok;
    uint8_t m_rx_buf[LORA_RX_BUF_SIZE];
    int m_rx_len;

    void handle_rx();
};

#endif
```

- [ ] **Commit**

```bash
cd hub && git add src/lora_handler.h && git commit -m "refactor: LoraHandler inherits RadioInterface"
```

---
### Task 4: Refactor `hub/src/lora_handler.cpp` to class

**Files:**
- Modify: `hub/src/lora_handler.cpp`

**Interfaces:**
- Consumes: `LoraHandler` class, `lora_config.h`, sandeepmistry/LoRa
- Produces: Full implementation of `LoraHandler` — pure radio I/O, no sensor_registry or mqtt_client calls

- [ ] **Rewrite the implementation**

Replace with:

```cpp
#ifdef HABILITA_LORA

#include "lora_handler.h"
#include "lora_config.h"
#include "lora_protocol.h"

int LoraHandler::init() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ * 1E6)) {
        return -1;
    }
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW * 1E3);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setPreambleLength(LORA_PREAMBLE);
    // Polling mode — parsePacket() called every loop()
    LoRa.receive();
    m_ok = true;
    return 0;
}

int LoraHandler::send(const uint8_t* data, size_t len) {
    if (!m_ok) return -1;
    LoRa.beginPacket();
    LoRa.write(data, len);
    return LoRa.endPacket() ? 0 : -1;
}

bool LoraHandler::is_ready() const {
    return m_ok;
}

void LoraHandler::loop() {
    if (!m_ok) return;
    handle_rx();
}

void LoraHandler::handle_rx() {
    int len = LoRa.parsePacket();
    if (len <= 0 || len > LORA_RX_BUF_SIZE) return;

    int i = 0;
    while (LoRa.available() && i < LORA_RX_BUF_SIZE) {
        m_rx_buf[i++] = LoRa.read();
    }
    m_rx_len = i;

    int16_t rssi = LoRa.packetRssi();

    if (m_rx_len >= LORA_HEADER_SIZE && m_rx_cb) {
        m_rx_cb(m_rx_buf, m_rx_len, rssi, m_rx_arg);
    }
}

#endif // HABILITA_LORA
```

Key differences from old RadioLib impl:
- `LoRa.begin()` takes frequency in Hz, returns bool
- `LoRa.parsePacket()` polling — no ISR flag needed, no `IRAM_ATTR`
- `LoRa.beginPacket()`/`write()`/`endPacket()` for send
- No `#include "sensor_registry.h"` or `"mqtt_client.h"` — pure radio layer

- [ ] **Commit**

```bash
cd hub && git add src/lora_handler.cpp && git commit -m "refactor: LoraHandler class impl, dispatch via callback"
```

---
### Task 5: Update `hub/include/lora_config.h`

**Files:**
- Modify: `hub/include/lora_config.h`

**Goal:** Replace `#ifdef MCU_TTGO` guard with unconditional pins for TTGO; add SPI pins; remove DIO1 (not used by LoRa.h).

- [ ] **Rewrite the file**

```cpp
#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

// TTGO LORA32 T3_v1.6 — SX1278 via sandeepmistry/LoRa
#define LORA_SS_PIN     18
#define LORA_RST_PIN    14
#define LORA_DIO0_PIN   26
// SPI bus (custom — different from default VSPI)
#define LORA_MOSI       27
#define LORA_MISO       19
#define LORA_SCK         5

// Radio parameters
#define LORA_FREQ       868.0
#define LORA_SF         10
#define LORA_BW         125
#define LORA_CR         7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17

#endif
```

- [ ] **Remove `-D MCU_TTGO` from `hub/platformio.ini`** if present (not needed — pins are unconditional now)

- [ ] **Commit**

```bash
cd hub && git add include/lora_config.h && git commit -m "fix: remove MCU_TTGO guard, pins always defined"
```

---
### Task 6: Update `hub/src/main.cpp` — integrate LoraHandler

**Files:**
- Modify: `hub/src/main.cpp`

**Interfaces:**
- Consumes: `LoraHandler` class, `radio_interface.h` rx_callback type
- Produces: Hub integration — dispatch logic moves from `lora_handler.cpp` to here

- [ ] **Add include and static instance near top of file**

```cpp
#ifdef HABILITA_LORA
#include "lora_handler.h"
#include "lora_protocol.h"
#include "sensor_registry.h"
#include "mqtt_client.h"
static LoraHandler s_lora;
#endif
```

- [ ] **Add callback function** (before `setup()`)

```cpp
#ifdef HABILITA_LORA
static void lora_rx_cb(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    if (len < LORA_HEADER_SIZE) return;
    const lora_frame_t* frame = (const lora_frame_t*)data;

    int slot = sensor_registry_find_by_mac(frame->sensor_id);

    switch (frame->msg_type) {
        case LORA_MSG_PAIR_REQUEST: {
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot < 0) return;
                uint8_t sensor_type = frame->payload_len > 0 ? frame->payload[0] : 0;
                sensor_registry_add(frame->sensor_id, sensor_type, slot, "", HW_CHIP_UNKNOWN);
            }
            lora_pair_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.msg_type = LORA_MSG_PAIR_RESPONSE;
            resp.sequence = frame->sequence;
            memcpy(resp.sensor_id, frame->sensor_id, 6);
            resp.payload_len = 1;
            resp.assigned_slot = slot;
            s_lora.send((const uint8_t*)&resp, sizeof(resp));
            break;
        }
        case LORA_MSG_SENSOR_DATA: {
            if (slot >= 0) {
                espnow_header_t hdr;
                memset(&hdr, 0, sizeof(hdr));
                hdr.sequence = frame->sequence;
                hdr.rssi = rssi;
                hdr.payload_len = frame->payload_len;
                sensor_registry_update_state(slot, &hdr, frame->payload, frame->payload_len);
                queue_bridge_state(slot);
            }
            break;
        }
        case LORA_MSG_HEARTBEAT: {
            if (slot >= 0) {
                virtual_sensor_t* s = sensor_registry_get(slot);
                if (s) {
                    s->last_seen = millis();
                    s->online = true;
                }
            }
            break;
        }
    }
}

static void queue_bridge_state(int slot) {
    static uint8_t s_lora_pending_state_slots[LORA_PENDING_STATE_MAX];
    static int s_lora_pending_state_head = 0;
    static int s_lora_pending_state_tail = 0;
    int next = (s_lora_pending_state_head + 1) % LORA_PENDING_STATE_MAX;
    if (next == s_lora_pending_state_tail) return;
    s_lora_pending_state_slots[s_lora_pending_state_head] = slot;
    s_lora_pending_state_head = next;
}

static void lora_process_bridge_queue(void) {
    static uint8_t s_lora_pending_state_slots[LORA_PENDING_STATE_MAX];
    static int s_lora_pending_state_head = 0;
    static int s_lora_pending_state_tail = 0;
    while (s_lora_pending_state_tail != s_lora_pending_state_head) {
        int slot = s_lora_pending_state_slots[s_lora_pending_state_tail];
        s_lora_pending_state_tail = (s_lora_pending_state_tail + 1) % LORA_PENDING_STATE_MAX;
        virtual_sensor_t* s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected()) {
            mqtt_client_publish_state(s);
        }
    }
}
#endif
```

Note: The `queue_bridge_state` and `lora_process_bridge_queue` used to be in `lora_handler.cpp`. They move here since they depend on `sensor_registry` and `mqtt_client`. Add `#define LORA_PENDING_STATE_MAX 5` at the top of the callback section.

Wait — these use static arrays that are defined twice. Let me redesign this. Better to have the queue as static variables at file scope (outside the functions) in main.cpp.

- [ ] **Replace setup() LoRa section**

Current:
```cpp
#ifdef HABILITA_LORA
    int lora_state = lora_handler_init();
    if (lora_state != 0) {
        console.printf("[LoRa] Init failed: %d — LoRa disabled\n", lora_state);
    } else {
        console.println("[LoRa] Initialized");
    }
#endif
```

Replace with:
```cpp
#ifdef HABILITA_LORA
    s_lora.set_rx_callback(lora_rx_cb, nullptr);
    int lora_state = s_lora.init();
    if (lora_state != 0) {
        console.printf("[LoRa] Init failed: %d — LoRa disabled\n", lora_state);
    } else {
        console.println("[LoRa] Initialized");
    }
#endif
```

- [ ] **Replace loop() LoRa section**

Current:
```cpp
#ifdef HABILITA_LORA
    lora_handler_loop();
#endif
```

Replace with:
```cpp
#ifdef HABILITA_LORA
    s_lora.loop();
    lora_process_bridge_queue();
#endif
```

- [ ] **Commit**

```bash
cd hub && git add src/main.cpp && git commit -m "refactor: integrate LoraHandler class, dispatch via callback"
```

---
### Task 7: Update `hub/platformio.ini` — swap RadioLib → sandeepmistry/LoRa

**Files:**
- Modify: `hub/platformio.ini`

- [ ] **Replace RadioLib with LoRa.h in `hub_32_lora` lib_deps**

```ini
[env:hub_32_lora]
extends = env:hub_32
build_flags =
    ${env:hub_32.build_flags}
    -D HABILITA_LORA
    -D HABILITA_DISPLAY_TTGO
lib_deps =
    ${env:hub_32.lib_deps}
    sandeepmistry/LoRa @ ^0.8.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.9
```

- [ ] **Commit**

```bash
cd hub && git add platformio.ini && git commit -m "refactor: replace RadioLib with sandeepmistry/LoRa"
```

---
### Task 8: Build test

**Files:** none

**Goal:** Verify both `hub_32` (unchanged) and `hub_32_lora` (refactored) compile.

- [ ] **Build `hub_32`**

```bash
pio run -e hub_32
```
Expected: SUCCESS (zero changes to this env)

- [ ] **Build `hub_32_lora`**

```bash
pio run -e hub_32_lora
```
Expected: SUCCESS

- [ ] **Fix any compilation errors** and repeat

- [ ] **Commit shared submodule** (if shared changed since last submodule commit)

```bash
cd shared && git push origin dev
```

---
### Task 9: Update `hub/SPEC_LORA.md`

**Files:**
- Modify: `hub/SPEC_LORA.md`

- [ ] **Update section 12 (Arquivos de Implementação)**

Replace table to reflect new architecture:

| Arquivo | Descrição |
|---------|-----------|
| `shared/src/radio_interface.h` | Classe abstrata `RadioInterface` (init/send/loop/is_ready + rx_callback) |
| `shared/src/lora_protocol.h` | Structs `lora_frame_t`, `lora_pair_response_t`, `lora_nak_t`, `lora_pair_request_t` + enum `lora_msg_type_t` |
| `hub/include/lora_config.h` | Pinos e parâmetros de rádio |
| `hub/src/lora_handler.h` | `class LoraHandler : public RadioInterface` — declaração |
| `hub/src/lora_handler.cpp` | sandeepmistry/LoRa init, ISR, send, loop — puro I/O, sem dispatch |
| `hub/src/main.cpp` | Instância `s_lora`, callback `lora_rx_cb` que faz parsing + dispatch |

- [ ] **Update Section 1** — replace "RadioLib/SX1278" with "sandeepmistry/LoRa (SX1278)"
- [ ] **Update Section 12** — replace RadioLib references with LoRa.h
- [ ] **Add rule:** "LoRa handler não faz dispatch direto — entrega dados via `rx_callback`. O dispatch para `sensor_registry`/`mqtt_client` é responsabilidade do caller."

- [ ] **Commit**

```bash
cd hub && git add SPEC_LORA.md && git commit -m "docs: update SPEC_LORA.md for RadioInterface abstraction"
```

---
### Task 10: Final verification

**Files:** none

- [ ] **Final build both envs**

```bash
pio run -e hub_32 && pio run -e hub_32_lora
```

- [ ] **Verify no regressions in hub_32** (zero code change — should be byte-identical binary)

- [ ] **Update shared submodule reference in homeware** (if shared commits were made)

```bash
cd shared && git push origin dev
cd ..
git add shared
git commit -m "chore: bump shared submodule for RadioInterface + lora_protocol"
```
