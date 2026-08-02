# Unified MSG_TYPE Implementation Plan

## Overview

Consolidate message type definitions from ESP-NOW, LoRa, and TCP protocols into a single shared `msg_type.h`.

## Design Spec

`docs/superpowers/specs/2026-08-01-unified-msg-type-design.md`

## Tasks

### Task 1: Create `shared/src/msg_type.h`

Create the unified enum file:

```cpp
#ifndef HW_SHARED_MSG_TYPE_H
#define HW_SHARED_MSG_TYPE_H

#include <stdint.h>

enum msg_type_t : uint8_t {
    MSG_SENSOR_DATA     = 0x01,
    MSG_PAIR_REQUEST    = 0x02,
    MSG_PAIR_RESPONSE   = 0x03,
    MSG_ACK             = 0x04,
    MSG_HEARTBEAT       = 0x05,
    MSG_OTA_TRIGGER     = 0x06,
    MSG_COMMAND         = 0x07,
    MSG_TIME_SYNC       = 0x08,
    MSG_GW_ANNOUNCE     = 0x09,
    MSG_GW_DISCOVER     = 0x0A,
    MSG_REPEATER_STATUS = 0x0B,
    MSG_RESTART         = 0x0C,
    MSG_NAK             = 0x0D,
};

#endif
```

**Acceptance:** File compiles standalone.

---

### Task 2: Update `shared/src/espnow_protocol.h`

- Remove `espnow_msg_type_t` enum (lines 23-37)
- Add `#include "msg_type.h"`
- All `ESPNOW_MSG_*` references in this file → `MSG_*`

**Acceptance:** `hub_8266` builds.

---

### Task 3: Update `shared/src/lora_protocol.h`

- Remove `lora_msg_type_t` enum (lines 56-64)
- Add `#include "msg_type.h"`
- All `LORA_MSG_*` references in this file → `MSG_*`

**Acceptance:** `onoff-lora` builds.

---

### Task 4: Update `shared/src/tcp_protocol.h`

- Remove `TCP_MSG_*` defines (lines 7-15)
- Add `#include "msg_type.h"`
- Struct comments referencing `TCP_MSG_*` → `MSG_*`

**Acceptance:** `hub_8266` builds.

---

### Task 5: Update `shared/src/espnow_node_protocol.cpp`

Replace all `ESPNOW_MSG_*` → `MSG_*`:
- Line 159: `ESPNOW_MSG_PAIR_REQUEST` → `MSG_PAIR_REQUEST`
- Line 177: `ESPNOW_MSG_SENSOR_DATA` → `MSG_SENSOR_DATA`
- Line 195: `ESPNOW_MSG_HEARTBEAT` → `MSG_HEARTBEAT`
- Line 214: `ESPNOW_MSG_PAIR_RESPONSE` → `MSG_PAIR_RESPONSE`
- Line 227: `ESPNOW_MSG_ACK` → `MSG_ACK`
- Line 237: `ESPNOW_MSG_NAK` → `MSG_NAK`
- Line 244: `ESPNOW_MSG_RESTART` → `MSG_RESTART`
- Line 251: `ESPNOW_MSG_COMMAND` → `MSG_COMMAND`
- Line 258: `ESPNOW_MSG_TIME_SYNC` → `MSG_TIME_SYNC`

**Acceptance:** `hub_8266` builds.

---

### Task 6: Update `shared/src/lora_node_protocol.cpp`

Replace all `LORA_MSG_*` → `MSG_*`:
- Line 87: `LORA_MSG_PAIR_REQUEST` → `MSG_PAIR_REQUEST`
- Line 106: `LORA_MSG_SENSOR_DATA` → `MSG_SENSOR_DATA`
- Line 119: `LORA_MSG_HEARTBEAT` → `MSG_HEARTBEAT`
- Line 135: `LORA_MSG_PAIR_RESPONSE` → `MSG_PAIR_RESPONSE`
- Line 147: `LORA_MSG_COMMAND` → `MSG_COMMAND`

**Acceptance:** `onoff-lora` builds.

---

### Task 7: Update `hub/src/espnow_handler.cpp`

Replace all `ESPNOW_MSG_*` → `MSG_*`:
- Line 175: `ESPNOW_MSG_ACK` → `MSG_ACK`
- Line 189: `ESPNOW_MSG_PAIR_RESPONSE` → `MSG_PAIR_RESPONSE`
- Line 203: `ESPNOW_MSG_GW_ANNOUNCE` → `MSG_GW_ANNOUNCE`
- Line 236: `ESPNOW_MSG_COMMAND` → `MSG_COMMAND`
- Line 256: `ESPNOW_MSG_RESTART` → `MSG_RESTART`
- Line 267: `ESPNOW_MSG_TIME_SYNC` → `MSG_TIME_SYNC`
- Line 286: `ESPNOW_MSG_*` cases in switch → `MSG_*`
- Line 305: `ESPNOW_MSG_NAK` → `MSG_NAK`
- Line 341: `ESPNOW_MSG_SENSOR_DATA` → `MSG_SENSOR_DATA`
- Line 428: `ESPNOW_MSG_COMMAND` → `MSG_COMMAND`

**Acceptance:** `hub_8266` builds.

---

### Task 8: Update `hub/src/lora_handler.cpp`

Replace `LORA_MSG_COMMAND` → `MSG_COMMAND`:
- Line 34: `LORA_MSG_COMMAND` → `MSG_COMMAND`
- Line 44: `LORA_MSG_COMMAND` → `MSG_COMMAND`

**Acceptance:** `hub_8266` builds.

---

### Task 9: Update `hub/src/tcp_radio_handler.cpp`

Replace `TCP_MSG_*` → `MSG_*`:
- Line 321: `TCP_MSG_GW_DISCOVER` → `MSG_GW_DISCOVER`
- Line 329: `TCP_MSG_GW_ANNOUNCE` → `MSG_GW_ANNOUNCE`

**Acceptance:** `hub_8266` builds.

---

### Task 10: Update `hub/src/main.cpp`

Replace `LORA_MSG_*` → `MSG_*`:
- Line 168: switch cases `LORA_MSG_*` → `MSG_*`
- Line 183: `LORA_MSG_PAIR_RESPONSE` → `MSG_PAIR_RESPONSE`

**Acceptance:** `hub_8266` builds.

---

### Task 11: Update node source files

Replace `ESPNOW_MSG_*` → `MSG_*` in:
- `nodes/extender/src/main.cpp` (lines 177-567, ~15 occurrences)
- `nodes/presence-bat/src/main.cpp` (lines 146, 168)
- `nodes/soil-moisture/src/main.cpp` (lines 138, 160)

**Acceptance:** All node configs build.

---

### Task 12: Verify all builds

Run build for all affected configurations:
- `hub_8266` (ESP-NOW + TCP + LoRa hub)
- `nodes/tcp` (TCP node)
- `nodes/lamp` (ESP-NOW node)
- `nodes/presence` (ESP-NOW node)
- `nodes/extender` (ESP-NOW extender)
- `nodes/onoff-lora` (LoRa node)

**Acceptance:** All builds pass.

---

## Execution Order

Tasks 1-4 (headers) must be done first, then tasks 5-11 (source files) can be parallelized, then task 12 (verification).

## Risk

LoRa protocol values change (HEARTBEAT 0x04→0x05, NAK 0x05→0x0D, GW_ANNOUNCE 0x06→0x09). Existing deployed LoRa nodes must be updated. Per AGENTS.md, LoRa nodes are "em desenvolvimento" — not yet in production.
