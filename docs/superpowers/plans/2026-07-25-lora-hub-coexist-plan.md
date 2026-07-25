# Lora Hub Coexist Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add LoRa (RadioLib/SX1278) as a coexisting transport alongside ESP-NOW on the TTGO LORA32 hub, controlled by `HABILITA_LORA` flag.

**Architecture:** `lora_handler` is added as a parallel module to `espnow_handler` in `hub/src/`. Both feed into the same `sensor_registry` → `mqtt_client` → HA pipeline. The espnow_handler remains untouched. LoRa protocol uses binary compact packets (11-byte overhead) with RadioLib SPI ISR.

**Tech Stack:** RadioLib `jgromes/RadioLib@^7.0`, SX1278, PlatformIO `esp32dev` board, Arduino framework.

## Global Constraints

- `HABILITA_LORA` compile flag controls all LoRa code — no LoRa code when flag is absent
- `hub_32` env PlatformIO remains 100% unchanged; new code only in `hub_32_lora` env
- `espnow_handler` files — zero modifications
- `sensor_registry` — transport-agnostic, no changes
- Loop non-blocking (regra 15): no `delay()` blocking in `lora_handler_loop()`
- `sensor_id` is 6 bytes (chip_id derived from MAC or EEPROM), matching ESP-NOW format
- All LoRa messages follow the binary protocol defined in design doc
- RadioLib ISR + SPI — `yield()` between operations

---

## Task 1: Create `lora_config.h`

**Files:**
- Create: `hub/include/lora_config.h`

**Interfaces:** Pino e frequência configuração LoRa para SX1278 no TTGO LORA32.

- [ ] **Step 1: Write `lora_config.h`**

```cpp
#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

#define LORA_SS_PIN     5
#define LORA_RST_PIN    14
#define LORA_DIO0_PIN   2
#define LORA_DIO1_PIN   15

#define LORA_FREQ       868.0
#define LORA_SF         10
#define LORA_BW         125
#define LORA_CR         7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17

#endif
```

- [ ] **Step 2: Commit**

```bash
git add hub/include/lora_config.h
git commit -m "feat: add lora_config.h for SX1278 pinos e paramêtros"
```

---

## Task 2: Create `lora_handler.h`

**Files:**
- Create: `hub/src/lora_handler.h`

**Interfaces:** Header declarations for LoRa handler module.

- [ ] **Step 1: Write `lora_handler.h`**

```cpp
#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#define LORA_MSG_SENSOR_DATA   0x01
#define LORA_MSG_PAIR_REQUEST  0x02
#define LORA_MSG_PAIR_RESPONSE 0x03
#define LORA_MSG_HEARTBEAT     0x04
#define LORA_MSG_NAK           0x05
#define LORA_MSG_GW_ANNOUNCE   0x06
#define LORA_MSG_COMMAND       0x07

#define LORA_RX_BUF_SIZE 256

bool lora_handler_init(void);
void lora_handler_loop(void);

#endif
```

- [ ] **Step 2: Commit**

```bash
git add hub/src/lora_handler.h
git commit -m "feat: add lora_handler.h declarations"
```

---

## Task 3: Create `lora_handler.cpp` — init + ISR + rx loop

**Files:**
- Create: `hub/src/lora_handler.cpp`

**Interfaces:** `lora_handler_init()` configura RadioLib, ISR de rx, `lora_handler_loop()` processa packetos recebidos.

**Depends on:** `lora_handler.h`, `lora_config.h`, `sensor_registry.h`

- [ ] **Step 1: Write `lora_handler.cpp` scaffold with includes and init**

Implement `lora_handler_init()`:
- `#include "lora_handler.h"`, `#include "lora_config.h"`, `#include "sensor_registry.h"`, `#include <RadioLib.h>`
- Include `platform.h` for `get_gateway_device_id()`
- Static `SX1278 radio = new Module(LORA_SS_PIN, LORA_DIO0_PIN, LORA_RST_PIN, LORA_DIO1_PIN)`
- Static `bool s_lora_rx_flag = false`
- Static `uint8_t s_lora_buf[LORA_RX_BUF_SIZE]`
- Static `int s_lora_buf_len = 0`
- `lora_handler_init()`:
  - `radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SS_PIN, LORA_DIO0_PIN, LORA_RST_PIN, LORA_DIO1_PIN)`
  - `radio.setCRCEnabled(true)`
  - `radio.setDio1Action(lora_rx_isr)`
  - `radio.startReceive()`
  - Return true on success, false on failure

- [ ] **Step 2: Add ISR and rx parsing functions**

Add `lora_rx_isr()` (static): sets `s_lora_rx_flag = true`

Add `lora_parse_packet()`:
- Read len from `radio.getPacketLength()`
- If len > 0 and len <= LORA_RX_BUF_SIZE:
  - `s_lora_buf_len = radio.readData(s_lora_buf, len)`
  - Extract fields from `s_lora_buf` (msg_type at offset 0, seq at 1-2, sensor_id at 3-8, rssi at 9, payload_len at 10, payload at 11)
  - Route by msg_type:
    - `LORA_MSG_PAIR_REQUEST` → process pair request → add to sensor_registry if slot free → send PAIR_RESPONSE
    - `LORA_MSG_SENSOR_DATA` → find slot by sensor_id → `sensor_registry_update_state(slot, ...)` → `queue_bridge_state(slot)`
    - `LORA_MSG_HEARTBEAT` → update `last_seen` and `online = true` for slot
    - Other → discard
  - Call `radio.startReceive()` again to re-enable RX

- [ ] **Step 3: Add `lora_handler_loop()`**

```cpp
void lora_handler_loop() {
    if (!s_lora_rx_flag) return;
    s_lora_rx_flag = false;
    lora_parse_packet();
}
```

- [ ] **Step 4: Commit**

```bash
git add hub/src/lora_handler.cpp hub/src/lora_handler.h
git commit -m "feat: implement lora_handler with RadioLib init, ISR rx, parser"
```

---

## Task 4: Integrate `lora_handler` into `main.cpp`

**Files:**
- Modify: `hub/src/main.cpp`

**Interfaces:** Add `#ifdef HABILITA_LORA` includes and calls to `lora_handler_init()` / `lora_handler_loop()`.

**Depends on:** `lora_handler.h` (Task 2)

- [ ] **Step 1: Add `#ifdef HABILITA_LORA` include block in main.cpp**

After `#include "log_buffer.h"` add:
```cpp
#ifdef HABILITA_LORA
#include "lora_handler.h"
#endif
```

- [ ] **Step 2: Add `lora_handler_init()` call in `setup()`**

Find `espnow_handler_init();` line. After it, add:
```cpp
#ifdef HABILITA_LORA
    if (!lora_handler_init()) {
        console.println("[LoRa] Init failed — LoRa disabled");
    } else {
        console.println("[LoRa] Initialized");
    }
#endif
```

- [ ] **Step 3: Add `lora_handler_loop()` call in `loop()`**

Find `espnow_handler_loop();` line. After it, add:
```cpp
#ifdef HABILITA_LORA
    lora_handler_loop();
#endif
```

- [ ] **Step 4: Commit**

```bash
git add hub/src/main.cpp
git commit -m "feat: integrate lora_handler into main.cpp with HABILITA_LORA guard"
```

---

## Task 5: Add `hub_32_lora` env to `platformio.ini`

**Files:**
- Modify: `hub/platformio.ini`

**Interfaces:** New PlatformIO build environment extending `hub_32` with LoRa dependencies.

- [ ] **Step 1: Add `hub_32_lora` env at end of `platformio.ini`**

Append at the end of `hub/platformio.ini`:
```ini
[env:hub_32_lora]
extends = env:hub_32
build_flags =
    -D HABILITA_LORA
lib_deps =
    bblanchon/ArduinoJson@^7.2.1
    knolleary/PubSubClient@^2.8
    jgromes/RadioLib@^7.0
```

- [ ] **Step 2: Commit**

```bash
git add hub/platformio.ini
git commit -m "feat: add hub_32_lora env with HABILITA_LORA and RadioLib"
```

---

## Task 6: Self-review and verify build compiles

**Verify all spec requirements are met:**

- [ ] `HABILITA_LORA` flag controls all LoRa code — no LoRa code compiled in `hub_32` env
- [ ] `hub_32` env unchanged (no new lib_deps, no new build_flags)
- [ ] `espnow_handler.cpp` zero modifications
- [ ] `lora_handler_loop()` is non-blocking — no `delay()` call
- [ ] Protocol binary packet format matches design doc (11-byte overhead, msg_types 0x01–0x07)
- [ ] `sensor_registry` unchanged — receives data from both ESP-NOW and LoRa paths
- [ ] Pinos LoRa match TTGO LORA32 pinout (SS=5, RST=14, DIO0=2, DIO1=15)
- [ ] RadioLib ISR uses DIO pin, not SPI blocking in loop
- [ ] `radio.startReceive()` re-enabled after each packet read
- [ ] Pair request path implemented (PAIR_REQUEST → find/create slot → PAIR_RESPONSE unicast)
- [ ] `queue_bridge_state(slot)` called for LoRa sensor data → same MQTT path as ESP-NOW

- [ ] **Step 1: Run PlatformIO build check for `hub_32` (no LoRa)**

Run (if PlatformIO available): `pio run -e hub_32 -t check` or `pio run -e hub_32` — should compile without RadioLib dependency.

- [ ] **Step 2: Run PlatformIO build check for `hub_32_lora`**

Run: `pio run -e hub_32_lora -t check` — should compile with RadioLib included. If build fails, check include paths and RadioLib API.

- [ ] **Step 3: Final commit if builds pass**

```bash
git add docs/superpowers/specs/2026-07-25-lora-hub-coexist-design.md
git commit -m "docs: verify LoRa hub design spec against implementation"
```

---

## Verification Checklist

Run these after completing all tasks:

1. `grep -r "HABILITA_LORA" hub/` — flag appears only in `lora_handler.cpp`, `lora_handler.h`, `main.cpp`, and `platformio.ini`
2. `grep -C1 "HABILITA_LORA" hub/src/main.cpp` — include guard and loop call present
3. `grep -C1 "HABILITA_LORA" hub/platformio.ini` — env `hub_32_lora` has the flag
4. `grep "RadioLib" hub/platformio.ini` — RadioLib dependency only in `hub_32_lora`
5. `grep "delay(" hub/src/lora_handler.cpp` — should return no results (no blocking delay)
6. `grep "espnow_handler" hub/src/lora_handler.cpp` — should return no results (no ESP-NOW coupling)
7. Hub env `hub_32` — `git diff` should show zero changes to existing files
