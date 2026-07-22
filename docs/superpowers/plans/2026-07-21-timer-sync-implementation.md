# Timer System + Repeater Shared + Lamp Sync — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate timer persistence from EEPROM to LittleFS, add cyclic timer, move repeater to shared, and add lamp sync (lampada only).

**Architecture:** Extend `shared/timer.h/.cpp` with LittleFS persistence and cyclic logic. Move repeater from lampada to `shared/common_repeater.h/.cpp`. Sync is lampada-only (needs gateway device_id routing).

**Tech Stack:** Arduino (ESP8266), ArduinoJson, LittleFS, ESP-NOW, PlatformIO

## Global Constraints

- ESP8266 with 4MB flash / 1MB LittleFS partition
- `shared/` submodule is the single source of cross-platform code
- All changes in `shared/` must work for both onoff and lampada
- FW_VERSION stays `v0.0.29` (no bump for in-progress work)
- No blocking `delay()` in loop — use millis()-based state machines
- Broadcast ESP-NOW for ESP8266→ESP32 (rule 18)

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `shared/src/timer.h` | Modify | Add cyclic struct, LittleFS API, pulse getters |
| `shared/src/timer.cpp` | Modify | LittleFS persistence, cyclic check, pulse config |
| `shared/src/common_repeater.h` | Create | Repeater types, API declarations |
| `shared/src/common_repeater.cpp` | Create | Repeater logic (from lampada) |
| `clients/esp8266_onoff/src/main.cpp` | Modify | Use new timer API, shared repeater, cyclic loop |
| `clients/esp8266_onoff/include/config.h` | Modify | Add CYCLIC_CHECK_INTERVAL_MS |
| `clients/esp8266_onoff/include/pages.h` | Modify | Add cyclic UI to dashboard |
| `clients/esp8266_lampada/src/main.cpp` | Modify | Use new timer API, shared repeater, cyclic + sync |
| `clients/esp8266_lampada/include/config.h` | Modify | Add CYCLIC_CHECK_INTERVAL_MS |
| `clients/esp8266_lampada/include/pages.h` | Modify | Add cyclic + sync UI to dashboard |

---

### Task 1: Move repeater from lampada to shared

**Files:**
- Create: `shared/src/common_repeater.h`
- Create: `shared/src/common_repeater.cpp`

**Interfaces:**
- Produces: `repeater_init()`, `repeater_forward()`, `repeater_track()`, `repeater_is_enabled()`, `repeater_set_enabled()`, `repeater_get_fwd_count()`, `repeater_get_clients()`, `repeater_get_client_count()`, `repeater_reset_stats()`, `repeater_save_enable()`, `repeater_load_enable()`

- [ ] **Step 1: Create common_repeater.h**

```cpp
#ifndef HW_SHARED_COMMON_REPEATER_H
#define HW_SHARED_COMMON_REPEATER_H

#include <stdint.h>

#define REPEATER_MAX_CLIENTS 5

typedef struct {
    uint8_t mac[6];
    uint32_t pkt_count;
} repeater_client_t;

bool repeater_init(uint16_t eeprom_addr);
void repeater_forward(const uint8_t *src_mac, const uint8_t *data, int len,
                      const uint8_t *gateway_mac, const uint8_t *broadcast_mac,
                      void (*send_fn)(const uint8_t *mac, const uint8_t *data, int len, const char *tag),
                      const char *tag);
bool repeater_is_enabled(void);
void repeater_set_enabled(bool enabled);
void repeater_save_enable(void);
void repeater_load_enable(void);
uint32_t repeater_get_fwd_count(void);
const repeater_client_t* repeater_get_clients(void);
int repeater_get_client_count(void);
void repeater_reset_stats(void);

#endif
```

- [ ] **Step 2: Create common_repeater.cpp**

Move logic from lampada's `track_repeater_client()`, `save_repeater_en()`, `load_repeater_en()`, and the forwarding block in `espnow_recv_cb()`:

```cpp
#include "common_repeater.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

static bool s_repeater_enabled = false;
static uint32_t s_repeater_fwd = 0;
static repeater_client_t s_rep_clients[REPEATER_MAX_CLIENTS];
static int s_rep_client_num = 0;
static uint16_t s_eeprom_addr = 0;

static void track_client(const uint8_t *mac) {
    for (int i = 0; i < s_rep_client_num; i++) {
        if (memcmp(s_rep_clients[i].mac, mac, 6) == 0) {
            s_rep_clients[i].pkt_count++;
            return;
        }
    }
    if (s_rep_client_num < REPEATER_MAX_CLIENTS) {
        memcpy(s_rep_clients[s_rep_client_num].mac, mac, 6);
        s_rep_clients[s_rep_client_num].pkt_count = 1;
        s_rep_client_num++;
    }
}

bool repeater_init(uint16_t eeprom_addr) {
    s_eeprom_addr = eeprom_addr;
    repeater_load_enable();
    return true;
}

void repeater_load_enable(void) {
    EEPROM.begin(256);
    s_repeater_enabled = (EEPROM.read(s_eeprom_addr) == 1);
    EEPROM.end();
}

void repeater_save_enable(void) {
    EEPROM.begin(256);
    EEPROM.write(s_eeprom_addr, s_repeater_enabled ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

bool repeater_is_enabled(void) { return s_repeater_enabled; }
void repeater_set_enabled(bool enabled) { s_repeater_enabled = enabled; }
uint32_t repeater_get_fwd_count(void) { return s_repeater_fwd; }
const repeater_client_t* repeater_get_clients(void) { return s_rep_clients; }
int repeater_get_client_count(void) { return s_rep_client_num; }

void repeater_reset_stats(void) {
    s_repeater_fwd = 0;
    for (int i = 0; i < s_rep_client_num; i++)
        s_rep_clients[i].pkt_count = 0;
}

void repeater_forward(const uint8_t *src_mac, const uint8_t *data, int len,
                      const uint8_t *gateway_mac, const uint8_t *broadcast_mac,
                      void (*send_fn)(const uint8_t *mac, const uint8_t *data, int len, const char *tag),
                      const char *tag) {
    if (!s_repeater_enabled) return;

    bool from_gateway = (memcmp(src_mac, gateway_mac, 6) == 0);
    if (from_gateway) {
        send_fn(broadcast_mac, data, len, tag);
    } else {
        track_client(src_mac);
        send_fn(broadcast_mac, data, len, tag);
    }
    s_repeater_fwd++;
}
```

- [ ] **Step 3: Add build_shared.py entry**

Ensure `common_repeater.cpp` is included in the build via `build_shared.py` or `library.json`.

- [ ] **Step 4: Commit**

```bash
git add shared/src/common_repeater.h shared/src/common_repeater.cpp
git commit -m "feat(shared): move repeater logic from lampada to common_repeater"
```

---

### Task 2: Extend timer.h with cyclic struct and LittleFS API

**Files:**
- Modify: `shared/src/timer.h`

**Interfaces:**
- Produces: `cyclic_config_t`, `timer_save_littlefs()`, `timer_load_littlefs()`, `cyclic_check()`, `cyclic_reset()`, `cyclic_get_enabled()`, `cyclic_get_duration()`, `cyclic_set_enabled()`, `cyclic_set_duration()`, `timer_pulse_get_enabled()`, `timer_pulse_get_duration()`, `timer_pulse_set_enabled()`, `timer_pulse_set_duration()`

- [ ] **Step 1: Add cyclic struct and new declarations**

```cpp
// Add after existing timer_config_t:
typedef struct __attribute__((packed)) {
    uint8_t  enabled;
    uint16_t duration_min;
} cyclic_config_t;

// LittleFS persistence
bool timer_save_littlefs(void);
bool timer_load_littlefs(void);

// Cyclic
int8_t cyclic_check(unsigned long now_ms, bool current_relay_state);
void cyclic_reset(void);
bool cyclic_get_enabled(void);
uint16_t cyclic_get_duration(void);
void cyclic_set_enabled(bool enabled);
void cyclic_set_duration(uint16_t min);

// Pulse config (persistence only — runtime logic stays in client)
bool timer_pulse_get_enabled(void);
uint16_t timer_pulse_get_duration(void);
void timer_pulse_set_enabled(bool enabled);
void timer_pulse_set_duration(uint16_t min);
```

- [ ] **Step 2: Commit**

```bash
git add shared/src/timer.h
git commit -m "feat(shared/timer): add cyclic struct, LittleFS API, pulse getters"
```

---

### Task 3: Implement LittleFS persistence + cyclic logic in timer.cpp

**Files:**
- Modify: `shared/src/timer.cpp`

**Interfaces:**
- Consumes: `timer_config_t`, `cyclic_config_t` from timer.h
- Produces: `timer_save_littlefs()`, `timer_load_littlefs()`, `cyclic_check()`, `cyclic_reset()`

- [ ] **Step 1: Add includes and static vars**

```cpp
#include <LittleFS.h>
#define TIMER_LITTLEFS_FILE "/timers.json"

static cyclic_config_t s_cyclic = {0, 30};
static struct { bool enabled; uint16_t duration_min; } s_pulse_cfg = {false, 60};
static bool s_cyclic_waiting_off = false;
static unsigned long s_cyclic_next_event = 0;
```

- [ ] **Step 2: Implement timer_load_littlefs()**

Read `/timers.json`, parse agenda timers, cyclic, and pulse config. Populate static vars.

- [ ] **Step 3: Implement timer_save_littlefs()**

Write all static vars to `/timers.json` (agenda array + cyclic + pulse objects).

- [ ] **Step 4: Implement cyclic_check()**

Returns +1 (ON), -1 (OFF), 0 (no change). Uses millis-based countdown. Resets on `cyclic_reset()`.

- [ ] **Step 5: Implement pulse getters/setters**

```cpp
bool timer_pulse_get_enabled(void) { return s_pulse_cfg.enabled; }
uint16_t timer_pulse_get_duration(void) { return s_pulse_cfg.duration_min; }
void timer_pulse_set_enabled(bool enabled) { s_pulse_cfg.enabled = enabled; }
void timer_pulse_set_duration(uint16_t min) { s_pulse_cfg.duration_min = min; }
```

- [ ] **Step 6: Update timer_to_json() and timer_from_json()**

Include cyclic and pulse in JSON serialization/deserialization.

- [ ] **Step 7: Commit**

```bash
git add shared/src/timer.cpp
git commit -m "feat(shared/timer): implement LittleFS persistence, cyclic check, pulse config"
```

---

### Task 4: Update onoff — use shared repeater + new timer API

**Files:**
- Modify: `clients/esp8266_onoff/src/main.cpp`
- Modify: `clients/esp8266_onoff/include/config.h`

**Interfaces:**
- Consumes: `repeater_init()`, `repeater_forward()`, `repeater_is_enabled()`, `repeater_set_enabled()`, `repeater_save_enable()`, `timer_load_littlefs()`, `timer_save_littlefs()`, `cyclic_check()`, `cyclic_reset()`, `timer_pulse_get_enabled()`, `timer_pulse_get_duration()`

- [ ] **Step 1: Add includes and config**

```cpp
#include <LittleFS.h>
#include "common_repeater.h"
// config.h: add #define CYCLIC_CHECK_INTERVAL_MS 1000
```

- [ ] **Step 2: Remove inline repeater code**

Remove `s_rep_clients`, `s_rep_client_num`, `s_repeater_fwd`, `track_repeater_client()`, `save_repeater_en()`, `load_repeater_en()`, and the forwarding block in recv_cb. Replace with `repeater_forward()` call.

- [ ] **Step 3: Update setup()**

```cpp
repeater_init(EEPROM_REPEATER_EN_ADDR);
timer_init(EEPROM_TIMER_BASE, MAX_TIMERS);
if (!timer_load_littlefs()) {
    timer_load();  // EEPROM migration
    timer_save_littlefs();
}
s_pulse_enabled = timer_pulse_get_enabled();
s_pulse_duration_min = timer_pulse_get_duration();
```

- [ ] **Step 4: Update set_relay()**

Add `cyclic_reset()` on OFF transition.

- [ ] **Step 5: Add cyclic check to loop()**

```cpp
if (now - s_last_cyclic_check > CYCLIC_CHECK_INTERVAL_MS) {
    s_last_cyclic_check = now;
    int8_t ca = cyclic_check(now, s_relay_state);
    if (ca == 1) { console.printf("[%s] Cyclic ON\n", TAG); set_relay(true); }
    else if (ca == -1) { console.printf("[%s] Cyclic OFF\n", TAG); set_relay(false); }
}
```

- [ ] **Step 6: Update API handlers**

Replace `timer_save()` with `timer_save_littlefs()`. Update `/api/repeater` to use shared API. Update `/api/timers` to include cyclic/pulse in response.

- [ ] **Step 7: Update pulse save calls**

Replace direct EEPROM writes for pulse with `timer_pulse_set_enabled()` / `timer_pulse_set_duration()` + `timer_save_littlefs()`.

- [ ] **Step 8: Build and verify**

Run: `cd /mnt/c/git/homeware/clients/esp8266_onoff && platformio run`

- [ ] **Step 9: Commit**

```bash
git add clients/esp8266_onoff/
git commit -m "feat(onoff): use shared repeater, LittleFS timers, cyclic support"
```

---

### Task 5: Update lampada — use shared repeater + new timer API + sync

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp`
- Modify: `clients/esp8266_lampada/include/config.h`

**Interfaces:**
- Consumes: Same as Task 4 + `sync_get_config()`, `sync_set_config()` (from timer module or local)

- [ ] **Step 1: Add includes and config**

Same as Task 4 Step 1.

- [ ] **Step 2: Remove inline repeater code**

Same as Task 4 Step 2 — replace with shared repeater API.

- [ ] **Step 3: Update setup()**

Same as Task 4 Step 3.

- [ ] **Step 4: Update set_relay() with cyclic reset + sync**

Add `cyclic_reset()` on OFF. Add sync forwarding: on any state change, if sync enabled, send `ESPNOW_MSG_COMMAND` to gateway with target device_id.

- [ ] **Step 5: Add cyclic check to loop()**

Same as Task 4 Step 5.

- [ ] **Step 6: Update API handlers**

Same as Task 4 Step 6, plus add `/api/sync` or include sync in `/api/timers`.

- [ ] **Step 7: Build and verify**

Run: `cd /mnt/c/git/homeware/clients/esp8266_lampada && platformio run`

- [ ] **Step 8: Commit**

```bash
git add clients/esp8266_lampada/
git commit -m "feat(lampada): use shared repeater, LittleFS timers, cyclic + sync"
```

---

### Task 6: Update onoff dashboard — add cyclic UI

**Files:**
- Modify: `clients/esp8266_onoff/include/pages.h`

- [ ] **Step 1: Add cyclic HTML section in "Ciclo"**

Toggle enable + duration input + status display.

- [ ] **Step 2: Add fetchCyclic() / saveCyclic() JavaScript**

- [ ] **Step 3: Update fetchTimers() to include cyclic**

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_onoff/include/pages.h
git commit -m "feat(onoff/dashboard): add cyclic timer UI"
```

---

### Task 7: Update lampada dashboard — add cyclic + sync UI

**Files:**
- Modify: `clients/esp8266_lampada/include/pages.h`

- [ ] **Step 1: Add cyclic HTML section in "Ciclo"**

Same as Task 6 Step 1.

- [ ] **Step 2: Add sync HTML section**

Toggle enable + target_id input + status display.

- [ ] **Step 3: Add fetchCyclic() / saveCyclic() / fetchSync() / saveSync() JavaScript**

- [ ] **Step 4: Update fetchTimers() to include cyclic + sync**

- [ ] **Step 5: Build and verify**

- [ ] **Step 6: Commit**

```bash
git add clients/esp8266_lampada/include/pages.h
git commit -m "feat(lampada/dashboard): add cyclic + sync UI"
```

---

### Task 8: Gateway — device_id-based command routing (sync support)

**Files:**
- Modify: `shared/src/espnow_protocol.h`
- Modify: `gateway/src/main.cpp`

**Interfaces:**
- Consumes: `ESPNOW_MSG_COMMAND` with `target_device_id` field
- Produces: Forwards command to target device MAC from registry

- [ ] **Step 1: Extend espnow_command_t**

Add `target_device_id[32]` to command struct.

- [ ] **Step 2: Update gateway recv_cb**

If `target_mac` is zero but `target_device_id` is set, resolve MAC from `sensor_registry` and forward.

- [ ] **Step 3: Build gateway**

- [ ] **Step 4: Commit**

```bash
git add shared/src/espnow_protocol.h gateway/
git commit -m "feat(gateway): device_id-based command routing for lamp sync"
```

---

### Task 9: EEPROM cleanup after migration

**Files:**
- Modify: `clients/esp8266_onoff/src/main.cpp`
- Modify: `clients/esp8266_lampada/src/main.cpp`

- [ ] **Step 1: Clear EEPROM timer/pulse regions after successful LittleFS load**

- [ ] **Step 2: Build both clients**

- [ ] **Step 3: Commit**

```bash
git commit -m "chore: clear EEPROM timer/pulse regions after LittleFS migration"
```

---

### Task 10: Integration test

- [ ] **Step 1: Flash onoff — verify agenda, cyclic, pulse, reboot persistence, OTA persistence**
- [ ] **Step 2: Flash lampada — same + sync test**
- [ ] **Step 3: Mark stable if all pass**

---

## Open Items

1. **Sync requires gateway device_id routing** (Task 8): Most complex part. Gateway needs to resolve `target_device_id` → MAC from `sensor_registry`. May need separate spec if scope grows.

2. **ESP-NOW payload size**: Adding `target_device_id[32]` to `espnow_command_t` — verify fits in 250 byte limit. Current ~8 bytes → ~40 bytes — OK.

3. **Pulse runtime vs persistence**: Pulse config (enabled, duration) stored in LittleFS via timer module. Actual countdown/auto-OFF logic stays in each client's `loop()`. Timer module provides persistence; client provides runtime.
