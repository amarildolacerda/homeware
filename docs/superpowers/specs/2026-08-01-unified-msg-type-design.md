# Unified MSG_TYPE Design

## Problem

Each protocol (ESP-NOW, LoRa, TCP) defines its own message type enum/defines with duplicated values and inconsistent naming. This creates maintenance burden and confusion.

## Solution

Create `shared/src/msg_type.h` with a single `msg_type_t` enum. All protocols reference this shared enum.

## Unified Enum

```cpp
enum msg_type_t {
    MSG_SENSOR_DATA     = 0x01,  // all protocols
    MSG_PAIR_REQUEST    = 0x02,  // all protocols
    MSG_PAIR_RESPONSE   = 0x03,  // all protocols
    MSG_ACK             = 0x04,  // ESP-NOW only
    MSG_HEARTBEAT       = 0x05,  // all protocols (unified)
    MSG_OTA_TRIGGER     = 0x06,  // ESP-NOW only
    MSG_COMMAND         = 0x07,  // all protocols
    MSG_TIME_SYNC       = 0x08,  // ESP-NOW + TCP
    MSG_GW_ANNOUNCE     = 0x09,  // all protocols (unified)
    MSG_GW_DISCOVER     = 0x0A,  // ESP-NOW + TCP
    MSG_REPEATER_STATUS = 0x0B,  // ESP-NOW only
    MSG_RESTART         = 0x0C,  // ESP-NOW + TCP
    MSG_NAK             = 0x0D,  // all protocols (unified)
};
```

## Breaking Changes

LoRa protocol values change:
- HEARTBEAT: 0x04 → 0x05
- NAK: 0x05 → 0x0D
- GW_ANNOUNCE: 0x06 → 0x09

**Impact:** Existing deployed LoRa nodes must be updated. LoRa nodes are not yet in production (per AGENTS.md, `nodes/onoff-lora` is "em desenvolvimento").

## Files to Modify

1. **Create:** `shared/src/msg_type.h` — unified enum
2. **Modify:** `shared/src/espnow_protocol.h` — remove `espnow_msg_type_t`, include `msg_type.h`, rename `ESPNOW_MSG_*` → `MSG_*`
3. **Modify:** `shared/src/lora_protocol.h` — remove `lora_msg_type_t`, include `msg_type.h`, rename `LORA_MSG_*` → `MSG_*`
4. **Modify:** `shared/src/tcp_protocol.h` — remove `TCP_MSG_*` defines, include `msg_type.h`, rename `TCP_MSG_*` → `MSG_*`
5. **Modify:** `shared/src/espnow_node_protocol.cpp` — update references
6. **Modify:** `shared/src/lora_node_protocol.cpp` — update references
7. **Modify:** `hub/src/espnow_handler.cpp` — update references
8. **Modify:** `hub/src/lora_handler.cpp` — update references
9. **Modify:** `hub/src/tcp_radio_handler.cpp` — update references
10. **Modify:** `hub/src/main.cpp` — update references
11. **Modify:** `nodes/extender/src/main.cpp` — update references
12. **Modify:** `nodes/presence-bat/src/main.cpp` — update references
13. **Modify:** `nodes/soil-moisture/src/main.cpp` — update references
14. **Modify:** `nodes/tcp/include/config.h` — update references

## Naming Convention

- Enum: `msg_type_t`
- Values: `MSG_SENSOR_DATA`, `MSG_PAIR_REQUEST`, etc.
- No protocol prefix (ESP-NOW, LoRa, TCP) in the unified enum

## Approach

1. Create `msg_type.h`
2. Update each protocol header to include `msg_type.h` and remove old enum/defines
3. Update all source files to use new names
4. Build and test all configurations
