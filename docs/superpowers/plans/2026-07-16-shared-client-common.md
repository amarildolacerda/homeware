# Shared Client Common Code Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unify duplicated client code (console telnet, uptime formatting, OTA, scheduling timer, captive-portal/WiFi) into `shared/` so all ESP8266 clients (chuva, dht_gas, lampada, onoff, pir, repeater) consume a single source, per AGENTS.md regra 17.

**Architecture:** Create focused shared modules (one responsibility each, per AGENTS.md): `common_console.h/.cpp` (telnet), `common_ota.h/.cpp` (ArduinoOTA), `common_util.h/.cpp` (`uptime_to_str`), `common_wifi.h/.cpp` (WiFi delegators to myWiFiManager). Plus the parametrized `shared/timer.h` + `shared/timer.cpp` (scheduling). Each client includes only the modules it needs. No monolithic `common.h`. Each client drops its local `console.h/cpp` (and `timer.h/cpp` where present) and adds `-I../shared` to platformio.ini, then includes `common.h`/`timer.h`. WiFiManager captive portal is centralized in `myWiFiManager` with an optional device-name parameter callback. Lampada/onoff keep their bespoke AP/auto-reconnect flow but may opt into the shared portal.

**Tech Stack:** PlatformIO, Arduino framework (ESP8266), ESP8266WiFi, ArduinoOTA, ArduinoJson, WiFiManager (tzapu). Shared code must stay cross-platform (ESP8266/ESP32) â€” use `ARDUINO_ARCH_ESP32` guards like the existing `myWiFiManager`.

## Global Constraints

- Code changes only on branch `dev` (AGENTS.md). Verify with `git branch --show-current`.
- `shared/` is the single source for protocol/WiFi/common code (regra 17 estendida). No divergent copies in clients.
- PROGMEM / low-memory: keep heap usage minimal on ESP8266; `common.cpp` must not allocate large RAM buffers.
- Non-blocking loop: no blocking `delay()` in shared `loop()` paths (regra 15). `mywifi_loop()` already non-blocking; keep `common_wifi_loop()` non-blocking.
- Captive portal: ESP8266 via WiFiManager; ESP32 portal is handled separately (not in scope for clients yet â€” clients are ESP8266 only).
- `FW_VERSION` must be defined by each client's `config.h` (kept per-client).
- EEPROM offsets for device_name/gateway MAC differ per client today; timer EEPROM base must be passed as parameter, NOT a global define, so shared timer works for any client.

---

### Task 1: Parametrize shared timer (move timer into shared/)

**Files:**
- Create: `shared/timer.h`
- Create: `shared/timer.cpp`
- Test: build only (ESP8266 has no unit-test harness; verification = `pio run -d clients/esp8266_lampada` compiles)

**Interfaces:**
- Consumes: nothing shared (self-contained; takes EEPROM base + count + size as params)
- Produces:
  - `bool timer_init(uint16_t eeprom_base, uint8_t max_timers);`
  - `void timer_load();` `void timer_save();`
  - `bool timer_get(int index, timer_config_t *out);`
  - `bool timer_set(int index, const timer_config_t *cfg);`
  - `int8_t timer_check(unsigned long current_epoch, int timezone_offset);`
  - `void timer_get_next(unsigned long current_epoch, int tz, unsigned long *next_epoch, uint8_t *next_action);`
  - `void timer_to_json(JsonDocument &doc);`
  - `bool timer_from_json(JsonDocument &doc);`
  - `struct timer_config_t` (identical layout to current `timer.h`)

- [ ] **Step 1: Write `shared/timer.h`**

```cpp
#ifndef HW_SHARED_TIMER_H
#define HW_SHARED_TIMER_H

#include <stdint.h>
#include <ArduinoJson.h>

typedef struct __attribute__((packed)) {
    uint8_t hour;
    uint8_t minute;
    uint8_t action;       // 0=OFF, 1=ON
    uint8_t days_mask;    // bit0=Sun...bit6=Sat, 0=all
    bool enabled;
} timer_config_t;

// Must be called once at startup. eeprom_base = per-client base offset,
// max_timers = number of slots (keep <= 8 to fit EEPROM).
bool timer_init(uint16_t eeprom_base, uint8_t max_timers);
void timer_load();
void timer_save();
bool timer_get(int index, timer_config_t *out);
bool timer_set(int index, const timer_config_t *cfg);
int8_t timer_check(unsigned long current_epoch, int timezone_offset);
void timer_get_next(unsigned long current_epoch, int timezone_offset,
                    unsigned long *next_epoch, uint8_t *next_action);
void timer_to_json(JsonDocument &doc);
bool timer_from_json(JsonDocument &doc);

#endif
```

- [ ] **Step 2: Write `shared/timer.cpp`** (adapted from clients/esp8266_lampada/src/timer.cpp â€” remove `config.h`/`MAX_TIMERS`/`EEPROM_TIMER_BASE` globals, use params)

```cpp
#include "timer.h"
#include <Arduino.h>
#include <EEPROM.h>

static timer_config_t *s_timers = nullptr;
static uint16_t *s_last_fired_minute = nullptr;
static uint16_t s_eeprom_base = 0;
static uint8_t s_max_timers = 0;
static bool s_loaded = false;

bool timer_init(uint16_t eeprom_base, uint8_t max_timers) {
    s_eeprom_base = eeprom_base;
    s_max_timers = max_timers;
    s_timers = (timer_config_t *)calloc(max_timers, sizeof(timer_config_t));
    s_last_fired_minute = (uint16_t *)calloc(max_timers, sizeof(uint16_t));
    if (!s_timers || !s_last_fired_minute) return false;
    memset(s_timers, 0, max_timers * sizeof(timer_config_t));
    memset(s_last_fired_minute, 0xFF, max_timers * sizeof(uint16_t));
    timer_load();
    s_loaded = true;
    return true;
}

void timer_load() {
    EEPROM.begin(512);
    for (int i = 0; i < s_max_timers; i++) {
        int addr = s_eeprom_base + i * sizeof(timer_config_t);
        uint8_t *dst = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            dst[j] = EEPROM.read(addr + j);
    }
    EEPROM.end();
}

void timer_save() {
    EEPROM.begin(512);
    for (int i = 0; i < s_max_timers; i++) {
        int addr = s_eeprom_base + i * sizeof(timer_config_t);
        uint8_t *src = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            EEPROM.write(addr + j, src[j]);
    }
    EEPROM.commit();
    EEPROM.end();
}

bool timer_get(int index, timer_config_t *out) {
    if (index < 0 || index >= s_max_timers) return false;
    if (out) *out = s_timers[index];
    return true;
}

bool timer_set(int index, const timer_config_t *cfg) {
    if (index < 0 || index >= s_max_timers || !cfg) return false;
    s_timers[index] = *cfg;
    s_last_fired_minute[index] = 0xFF;
    return true;
}

int8_t timer_check(unsigned long current_epoch, int timezone_offset) {
    if (!s_loaded) return -1;
    if (current_epoch < 100000) return -1;
    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday;
    uint16_t now_minute_id = now_hour * 60 + now_min;
    for (int i = 0; i < s_max_timers; i++) {
        if (!s_timers[i].enabled) continue;
        uint16_t timer_minute_id = s_timers[i].hour * 60 + s_timers[i].minute;
        if (timer_minute_id != now_minute_id) continue;
        if (s_last_fired_minute[i] == now_minute_id) continue;
        if (s_timers[i].days_mask != 0) {
            if (!(s_timers[i].days_mask & (1 << now_wday))) continue;
        }
        s_last_fired_minute[i] = now_minute_id;
        return (int8_t)s_timers[i].action;
    }
    return -1;
}

void timer_get_next(unsigned long current_epoch, int timezone_offset,
                    unsigned long *next_epoch, uint8_t *next_action) {
    *next_epoch = 0; *next_action = 0;
    if (!s_loaded || current_epoch < 100000) return;
    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday;
    uint16_t now_slot = now_hour * 60 + now_min;
    unsigned long best_epoch = 0; uint8_t best_action = 0;
    for (int i = 0; i < s_max_timers; i++) {
        if (!s_timers[i].enabled) continue;
        uint16_t timer_slot = s_timers[i].hour * 60 + s_timers[i].minute;
        bool day_ok = (s_timers[i].days_mask == 0);
        if (!day_ok) {
            for (int d = 0; d < 7; d++)
                if (s_timers[i].days_mask & (1 << d)) { day_ok = true; break; }
        }
        if (!day_ok) continue;
        unsigned long candidate = current_epoch;
        if (timer_slot <= now_slot) candidate += 86400;
        candidate += (timer_slot - now_slot) * 60;
        candidate -= timezone_offset * 3600;
        if (best_epoch == 0 || candidate < best_epoch) {
            best_epoch = candidate; best_action = s_timers[i].action;
        }
    }
    *next_epoch = best_epoch; *next_action = best_action;
}

void timer_to_json(JsonDocument &doc) {
    JsonArray arr = doc["timers"].to<JsonArray>();
    for (int i = 0; i < s_max_timers; i++) {
        JsonObject t = arr.add<JsonObject>();
        t["hour"] = s_timers[i].hour;
        t["minute"] = s_timers[i].minute;
        t["action"] = s_timers[i].action;
        t["days_mask"] = s_timers[i].days_mask;
        t["enabled"] = s_timers[i].enabled;
    }
}

bool timer_from_json(JsonDocument &doc) {
    if (!doc.containsKey("timers")) return false;
    JsonArray arr = doc["timers"].as<JsonArray>();
    int count = arr.size();
    if (count > s_max_timers) count = s_max_timers;
    for (int i = 0; i < count; i++) {
        JsonObject t = arr[i];
        s_timers[i].hour = t["hour"] | 0;
        s_timers[i].minute = t["minute"] | 0;
        s_timers[i].action = t["action"] | 0;
        s_timers[i].days_mask = t["days_mask"] | 0;
        s_timers[i].enabled = t["enabled"] | false;
    }
    return true;
}
```

- [ ] **Step 3: Build lampada to confirm shared timer compiles standalone (dry include)**

Run: `pio run -d clients/esp8266_lampada -t clean; pio run -d clients/esp8266_lampada`
Expected: compiles (timer not yet wired, just confirms shared/ builds). If platformio not on PATH, use `python -m platformio ...`.

- [ ] **Step 4: Commit**

```bash
git add shared/timer.h shared/timer.cpp
git commit -m "feat(shared): add parametrized timer (EEPROM base + max count)"
```

---

### Task 2: Add captive-portal + device-name helper to myWiFiManager

**Files:**
- Modify: `shared/myWiFiManager.h` (add `mywifi_portal(const char *name_buf, size_t name_size, void (*on_name)(const char*))` declaration)
- Modify: `shared/myWiFiManager.cpp` (implement captive portal with device-name param, ESP8266 path; fix reconnect-with-saved-creds bug)
- Test: build gateway (uses myWiFiManager) â€” `pio run -d gateway`

**Interfaces:**
- Consumes: existing `sh_creds_load`, `EEPROM_*` defines from `shared_config.h`
- Produces:
  - `bool mywifi_portal(char *name_buf, size_t name_size, void (*on_name)(const char*))` â€” opens WiFiManager portal, registers a `dev_name` text param bound to `name_buf`, calls `on_name()` with the entered value on save. Returns true if connected.

- [ ] **Step 1: Add declaration to `shared/myWiFiManager.h`**

After the existing `bool mywifi_begin(bool force_portal);` line, add:

```cpp
// Abre portal cativo (WiFiManager no ESP8266) com campo "Device Name".
// name_buf: buffer que recebe o nome digitado (ja pre-preenchido).
// on_name: callback chamado com o nome validado ao salvar.
// Retorna true se conectou/portal concluido.
bool mywifi_portal(char *name_buf, size_t name_size, void (*on_name)(const char*));
```

- [ ] **Step 2: Fix reconnect bug in `mywifi_loop()` (ESP8266 path uses `WiFi.begin()` without creds)**

In `shared/myWiFiManager.cpp`, replace the ESP8266 reconnect block (`WiFi.begin();`) with saved-cred reconnect. Locate:

```cpp
        WiFi.begin();
        s_reconnect_active = true;
```

Replace with:

```cpp
        char ssid[EEPROM_WIFI_SSID_SIZE];
        char pass[EEPROM_WIFI_PASS_SIZE];
        if (sh_creds_load(ssid, pass)) {
            WiFi.begin(ssid, pass);
        } else {
            WiFi.begin();
        }
        s_reconnect_active = true;
```

- [ ] **Step 3: Implement `mywifi_portal` (ESP8266 only; ESP32 path returns false with TODO)**

At end of `shared/myWiFiManager.cpp`, before `mywifi_espnow_mac`, add:

```cpp
bool mywifi_portal(char *name_buf, size_t name_size, void (*on_name)(const char*)) {
#if defined(ARDUINO_ARCH_ESP32)
    return false; // ESP32 portal tratado separadamente
#else
    WiFiManager wm;
    wm.setConfigPortalTimeout(300);
    WiFiManagerParameter custom_dev_name("dev_name", "Device Name",
                                         name_buf ? name_buf : "", name_size > 0 ? (int)name_size : 32);
    wm.addParameter(&custom_dev_name);
    if (wm.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS)) {
        if (name_buf && on_name) {
            const char *v = custom_dev_name.getValue();
            if (v && v[0]) {
                strncpy(name_buf, v, name_size - 1);
                name_buf[name_size - 1] = '\0';
                on_name(name_buf);
            }
        }
        return true;
    }
    return false;
#endif
}
```

- [ ] **Step 4: Build gateway to verify myWiFiManager still compiles**

Run: `pio run -d gateway`
Expected: SUCCESS (4 envs compile).

- [ ] **Step 5: Commit**

```bash
git add shared/myWiFiManager.h shared/myWiFiManager.cpp
git commit -m "feat(shared): add captive portal with device-name + fix reconnect creds"
```

---

### Task 3: Wire shared/common + timer into esp8266_lampada

**Files:**
- Modify: `clients/esp8266_lampada/platformio.ini` (add `-I../shared` to each env build_flags)
- Delete: `clients/esp8266_lampada/include/console.h`, `clients/esp8266_lampada/src/console.cpp`
- Delete: `clients/esp8266_lampada/include/timer.h`, `clients/esp8266_lampada/src/timer.cpp`
- Modify: `clients/esp8266_lampada/src/main.cpp` (replace `#include "console.h"` and `#include "timer.h"` with `#include "common_console.h"`n#include "common_ota.h"`n#include "common_util.h"`n#include "common_wifi.h"` + `#include "timer.h"` from shared; replace `ArduinoOTA.*` setup with `ota_setup(s_device_id)`; replace `uptime_s`/`Up:` calc with `uptime_to_str`; call `timer_init(EEPROM_TIMER_BASE, MAX_TIMERS)` in setup)
- Test: `pio run -d clients/esp8266_lampada`

**Interfaces:** (consumes shared APIs from Tasks 1-2)

- [ ] **Step 1: Add `-I../shared` to platformio.ini**

Edit each `[env:...]` `build_flags` to append `-I../shared`. Example diff on the first env:

```
 build_flags =
     -I../shared
```

(keep existing flags; just add the include path line)

- [ ] **Step 2: Delete local console + timer files**

```bash
git rm clients/esp8266_lampada/include/console.h clients/esp8266_lampada/src/console.cpp clients/esp8266_lampada/include/timer.h clients/esp8266_lampada/src/timer.cpp
```

- [ ] **Step 3: Update main.cpp includes**

Replace:
```cpp
#include "console.h"
#include "timer.h"
```
with:
```cpp
#include "common_console.h"`n#include "common_ota.h"`n#include "common_util.h"`n#include "common_wifi.h"
#include "timer.h"
```

- [ ] **Step 4: Replace OTA setup block**

Replace the `ArduinoOTA.setHostname(...)` ... `ArduinoOTA.begin();` + `console.printf("[%s] OTA ready...` block (lines ~1693-1703) with:

```cpp
    ota_setup(s_device_id);
```

- [ ] **Step 5: Replace uptime calcs with `uptime_to_str`**

In `handle_api_state`: replace `doc["uptime_s"] = (millis() - s_start_time) / 1000;` with:
```cpp
    char upbuf[32];
    uptime_to_str(millis() - s_start_time, upbuf, sizeof(upbuf));
    doc["uptime"] = upbuf;
```
In `handle_console` status/help: replace `console.printf("  Up:       %lu s\n", (millis() - s_start_time) / 1000);` with:
```cpp
    char upbuf[32];
    uptime_to_str(millis() - s_start_time, upbuf, sizeof(upbuf));
    console.printf("  Up:       %s\n", upbuf);
```

- [ ] **Step 6: Init shared timer in setup**

In `setup()`, after `load_device_name();`, add:
```cpp
    timer_init(EEPROM_TIMER_BASE, MAX_TIMERS);
```
(EEPROM_TIMER_BASE / MAX_TIMERS remain defined in lampada's config.h.)

- [ ] **Step 7: Build**

Run: `pio run -d clients/esp8266_lampada`
Expected: SUCCESS. Fix any symbol errors (e.g. `console` now comes from common.h â€” ensure no duplicate `ConsoleOutput console;` definition; the local console.cpp was deleted so only shared defines it).

- [ ] **Step 8: Commit**

```bash
git add clients/esp8266_lampada
git commit -m "refactor(lampada): use shared/common_*.h + timer.h from shared/"
```

---

### Task 4: Wire shared/common into esp8266_onoff (has timer + console)

Repeat Task 3 pattern for onoff: add `-I../shared`, delete local console.h/cpp + timer.h/cpp, update includes, replace OTA + uptime, `timer_init(EEPROM_TIMER_BASE, MAX_TIMERS)` in setup, build, commit.

**Files:**
- Modify: `clients/esp8266_onoff/platformio.ini`
- Delete: `clients/esp8266_onoff/include/console.h`, `src/console.cpp`, `include/timer.h`, `src/timer.cpp`
- Modify: `clients/esp8266_onoff/src/main.cpp`
- Test: `pio run -d clients/esp8266_onoff`

Steps mirror Task 3 Steps 1-8 (adapt `s_device_id`/uptime locations to onoff's main.cpp). Build expected SUCCESS.

---

### Task 5: Wire shared/common into esp8266_chuva (console + WiFiManager portal)

**Files:**
- Modify: `clients/esp8266_chuva/platformio.ini` (add `-I../shared`)
- Delete: `clients/esp8266_chuva/include/console.h`, `src/console.cpp`
- Modify: `clients/esp8266_chuva/src/main.cpp`
  - Replace `#include "console.h"` with `#include "common_console.h"`n#include "common_ota.h"`n#include "common_util.h"`n#include "common_wifi.h"`
  - Replace local `wifi_setup()` portal block (the `WiFiManager wifiManager;` + `custom_dev_name` + `startConfigPortal`) with `mywifi_portal(s_device_name, sizeof(s_device_name), [](const char* n){ save_device_name(n); })`
  - Replace `ArduinoOTA.*` setup block with `ota_setup(s_device_id)`
  - Replace uptime calc in status with `uptime_to_str`
- Test: `pio run -d clients/esp8266_chuva`

- [ ] **Step 1-7 mirror Task 3 (no timer in chuva).** For the wifi_setup portal replacement, locate the block in `wifi_setup()`:

```cpp
    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 32);
    wifiManager.addParameter(&custom_dev_name);

    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0)
        {
            strncpy(s_device_name, custom_dev_name.getValue(), sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
            save_device_name(s_device_name);
        }
        s_wifi_configuration_mode = false;
        return true;
    }
```

Replace with:

```cpp
    if (mywifi_portal(s_device_name, sizeof(s_device_name),
                      [](const char *n) { save_device_name(n); }))
    {
        s_wifi_configuration_mode = false;
        return true;
    }
```

- [ ] **Step 8: Build** `pio run -d clients/esp8266_chuva` â†’ SUCCESS, commit `refactor(chuva): use shared/common_*.h + mywifi_portal`.

---

### Task 6: Wire shared/common into esp8266_dht_gas, esp8266_pir, esp8266_repeater (console only)

For each: add `-I../shared`, delete local `console.h`/`console.cpp`, replace `#include "console.h"` with `#include "common_console.h"`n#include "common_ota.h"`n#include "common_util.h"`n#include "common_wifi.h"`, replace `ArduinoOTA.*` setup with `ota_setup(s_device_id)`, replace uptime calcs with `uptime_to_str`, build, commit.

- dht_gas: `pio run -d clients/esp8266_dht_gas`
- pir: `pio run -d clients/esp8266_pir`
- repeater: `pio run -d clients/esp8266_repeater`

Each commit message: `refactor(<client>): use shared/common_*.h`.

---

### Task 7: Update AGENTS.md note + SPEC_ESPNOW cross-reference

**Files:**
- Modify: `AGENTS.md` (regra 17 estendida: add `shared/common.h`, `shared/common.cpp`, `shared/timer.h`, `shared/timer.cpp` to the single-source list)

- [ ] **Step 1: Edit AGENTS.md** â€” append to the regra 17 estendida paragraph:

```
; `shared/common.h`+`common.cpp` (console telnet, uptime_to_str, OTA, wifi delegators)
e `shared/timer.h`+`timer.cpp` (agendamento de timers por EEPROM base parametrizado).
```

- [ ] **Step 2: Commit**

```bash
git add AGENTS.md
git commit -m "docs: list shared common/timer in single-source rule"
```

---

## Self-Review

1. **Spec coverage:** All duplicated client code identified (console Ã—6, timer Ã—2, OTA Ã—6, WiFiManager portal Ã—1 chuva, uptime Ã—6) is addressed. Captive portal centralized in myWiFiManager (Task 2) â€” answers "jÃ¡ tem no myWiFiManager?" yes, now with device-name param.
2. **Placeholder scan:** No TBD/TODO except the explicit ESP32 `return false;` stub in `mywifi_portal` (documented as out-of-scope since clients are ESP8266 only). All code steps show concrete code.
3. **Type consistency:** `timer_init(uint16_t, uint8_t)`, `timer_config_t` layout unchanged from original `timer.h`. `ota_setup(const char*)`, `uptime_to_str(unsigned long, char*, size_t)` match declarations in `common.h`. `mywifi_portal` signature matches Task 2 declaration.

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-16-shared-client-common.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration
**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?

