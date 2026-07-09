# ESP8266 OnOff Client — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `esp8266_onoff` client with timer/scheduler, and extend gateway to broadcast NTP-synced time via ESP-NOW.

**Architecture:** Fork `esp8266_lampada` → add timer module + time sync ESP-NOW receive. Gateway gets NTP + periodic `ESPNOW_MSG_TIME_SYNC` broadcast. Client gets time exclusively via ESP-NOW (no NTP on client).

**Tech Stack:** ESP8266 Arduino, ESP-NOW, ArduinoJson, Espalexa

## Global Constraints

- Gateway files marked as stable in AGENTS.md must not break existing functionality — only add new message type + NTP broadcast
- FW_VERSION must remain consistent across all devices (currently v0.0.20)
- All state changes must set `s_last_espnow_send = 0` for immediate gateway feedback
- Loop must be non-blocking — no `delay()` in timer check

---

### Task 1: Add ESPNOW_MSG_TIME_SYNC to protocol

**Files:**
- Modify: `gateway/include/espnow_protocol.h:13-21` (add enum value)
- Modify: `gateway/include/espnow_protocol.h:113-118` (add struct)
- Modify: `clients/esp8266_lampada/include/espnow_protocol.h:13-21` (same change)
- Create: `clients/esp8266_onoff/include/espnow_protocol.h` (will be created in Task 4)

**Interfaces:**
- Consumes: existing protocol definitions
- Produces: `ESPNOW_MSG_TIME_SYNC = 0x08`, `espnow_time_sync_t` struct

- [ ] **Step 1: Add enum value**

In both `gateway/include/espnow_protocol.h` and `clients/esp8266_lampada/include/espnow_protocol.h`, line 20-21:

```c
    ESPNOW_MSG_COMMAND = 0x07,
    ESPNOW_MSG_TIME_SYNC = 0x08,
} espnow_msg_type_t;
```

- [ ] **Step 2: Add time sync struct**

In both files, after `espnow_command_t`:

```c
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t gateway_mac[6];
    uint32_t epoch_seconds;
} espnow_time_sync_t;
```

- [ ] **Step 3: Build verification**

```bash
cd /mnt/c/project/homeware/gateway
pio run 2>&1 | tail -5
```
Expected: `=== [SUCCESS] Took ...`

---

### Task 2: Gateway — NTP sync

**Files:**
- Modify: `gateway/include/config.h` — add NTP constants
- Modify: `gateway/src/main.cpp` — add NTP init in `setup()`, NTP check in `loop()`, add to `handle_console` status

**Interfaces:**
- Consumes: `WiFiUdp.h` (already included in main.cpp via `#include <WiFiUdp.h>`)
- Produces: `s_ntp_synced` flag, `s_ntp_epoch` time_t, `s_ntp_retry` timer for later tasks

- [ ] **Step 1: Add config constants**

In `gateway/include/config.h`, add before `#define MQTT_HOST_DEFAULT`:

```c
#define TIME_SYNC_INTERVAL_MS 300000
#define NTP_RETRY_INTERVAL_MS 1800000
```

- [ ] **Step 2: Add NTP state variables**

In `gateway/src/main.cpp`, after `static unsigned long s_last_telemetry = 0;`:

```c
static bool s_ntp_synced = false;
static unsigned long s_last_ntp_retry = 0;
static time_t s_ntp_epoch = 0;
```

- [ ] **Step 3: Add NTP init in setup**

In `gateway/src/main.cpp`, after `web_server_init();` call (after line 135):

```c
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    console.printf("[%s] NTP: pool.ntp.org, non-blocking sync\n", TAG);
```

- [ ] **Step 4: Add NTP retry to loop**

In `gateway/src/main.cpp` loop(), after the telemetry block (line 188):

```c
    if (!s_ntp_synced && millis() - s_last_ntp_retry > NTP_RETRY_INTERVAL_MS) {
        s_last_ntp_retry = millis();
        s_ntp_epoch = time(nullptr);
        if (s_ntp_epoch > 100000) {
            s_ntp_synced = true;
            struct tm *ti = localtime(&s_ntp_epoch);
            console.printf("[%s] NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                TAG, ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                ti->tm_hour, ti->tm_min, ti->tm_sec);
        } else {
            console.printf("[%s] NTP sync failed, will retry in %d min\n",
                TAG, NTP_RETRY_INTERVAL_MS / 60000);
        }
    }

    /* Update s_ntp_epoch every loop tick while synced */
    if (s_ntp_synced) {
        s_ntp_epoch = time(nullptr);
    }
```

- [ ] **Step 5: Update status command**

In `gateway/src/main.cpp`, inside case 's' (line 80-93), add after uptime line:

```c
            console.printf("NTP: %s\n", s_ntp_synced ? "sincronizado" : "aguardando...");
```

- [ ] **Step 6: Build verification**

```bash
cd /mnt/c/project/homeware/gateway
pio run 2>&1 | tail -5
```
Expected: SUCCESS

---

### Task 3: Gateway — ESP-NOW time sync broadcast

**Files:**
- Modify: `gateway/include/espnow_handler.h` — add time sync function
- Modify: `gateway/src/main.cpp` — call broadcast in loop
- Modify: `gateway/src/espnow_handler.cpp` — fix error: the s_last_time_sync is currently in main.cpp, we need the broadcast function in espnow_handler

Actually, let me think about this. The broadcast uses `esp_now_send` which is available in espnow_handler.cpp. Let me add the broadcast function there.

**Files:**
- Modify: `gateway/include/espnow_handler.h` — declare `espnow_broadcast_time_sync()`
- Modify: `gateway/src/espnow_handler.cpp` — implement broadcast
- Modify: `gateway/src/main.cpp` — call broadcast in loop

- [ ] **Step 1: Declare function**

In `gateway/include/espnow_handler.h`, add before `#endif`:

```c
void espnow_broadcast_time_sync(uint32_t epoch_seconds);
```

- [ ] **Step 2: Implement broadcast**

In `gateway/src/espnow_handler.cpp`, add before `espnow_get_gateway_mac`:

```c
static uint8_t s_time_sync_sequence = 0;
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void espnow_broadcast_time_sync(uint32_t epoch_seconds) {
    espnow_time_sync_t ts;
    memset(&ts, 0, sizeof(ts));
    ts.msg_type = ESPNOW_MSG_TIME_SYNC;
    ts.sequence = s_time_sync_sequence++;
    mac_copy(ts.gateway_mac, s_gateway_mac);
    ts.epoch_seconds = epoch_seconds;

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper((uint8_t*)s_broadcast_mac, ch);
    int ret = esp_now_send((uint8_t*)s_broadcast_mac, (uint8_t*)&ts, sizeof(ts));
    if (ret == 0) {
        console.printf("[ESP-NOW] Time sync broadcast sent (epoch=%u seq=%d)\n",
                      (unsigned)epoch_seconds, ts.sequence);
    } else {
        console.printf("[ESP-NOW] Time sync broadcast FAILED ret=%d\n", ret);
    }
}
```

- [ ] **Step 3: Call broadcast from main loop**

In `gateway/src/main.cpp`, add state for last broadcast time:

```c
static unsigned long s_last_time_sync = 0;
```

Add after the NTP retry block (inside loop, after the `s_ntp_epoch = time(nullptr);` line):

```c
    if (s_ntp_synced && millis() - s_last_time_sync > TIME_SYNC_INTERVAL_MS) {
        s_last_time_sync = millis();
        espnow_broadcast_time_sync((uint32_t)s_ntp_epoch);
    }
```

- [ ] **Step 4: Build and verify**

```bash
cd /mnt/c/project/homeware/gateway
pio run 2>&1 | tail -5
```
Expected: SUCCESS

---

### Task 4: Fork lampada → onoff directory structure

**Files:**
- Create: `clients/esp8266_onoff/` (entire directory copied from `clients/esp8266_lampada/`)
- Modify: `clients/esp8266_onoff/include/config.h` — update device name, add timer constants
- Create: `clients/esp8266_onoff/include/timer.h`
- Create: `clients/esp8266_onoff/src/timer.cpp`
- Modify: `clients/esp8266_onoff/src/main.cpp` — include timer.h, add variables
- Modify: `clients/esp8266_onoff/include/pages.h` — update title
- Modify: `clients/esp8266_onoff/README.md`, `clients/esp8266_onoff/SPEC.md` — update for onoff client

- [ ] **Step 1: Copy lampada to onoff**

```bash
cp -a /mnt/c/project/homeware/clients/esp8266_lampada /mnt/c/project/homeware/clients/esp8266_onoff
```

- [ ] **Step 2: Update config.h**

In `clients/esp8266_onoff/include/config.h`, change:

```c
#define FW_VERSION "v0.0.20"
#define DEVICE_NAME "OnOff"
```

Add after the existing constants (before `#define WIFI_CONFIG_PORTAL_SSID`):

```c
/* Timer */
#define MAX_TIMERS 6
#define TIMER_CHECK_INTERVAL_MS 10000
#define EEPROM_TIMEZONE_ADDR 160
#define EEPROM_TIMER_BASE 161
```

- [ ] **Step 3: Create timer.h**

Write `clients/esp8266_onoff/include/timer.h`:

```c
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <ArduinoJson.h>

#define MAX_TIMERS 6

typedef struct __attribute__((packed)) {
    uint8_t hour;
    uint8_t minute;
    uint8_t action;       // 0=OFF, 1=ON
    uint8_t days_mask;    // bit0=Sun...bit6=Sat, 0=all
    bool enabled;
} timer_config_t;

bool timer_init();
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

- [ ] **Step 4: Create timer.cpp**

Write `clients/esp8266_onoff/src/timer.cpp`:

```c
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "timer.h"

static timer_config_t s_timers[MAX_TIMERS];
static uint8_t s_last_fired_minute[MAX_TIMERS]; // track per-timer to avoid re-fire
static bool s_timers_loaded = false;

bool timer_init() {
    memset(s_timers, 0, sizeof(s_timers));
    memset(s_last_fired_minute, 0xFF, sizeof(s_last_fired_minute));
    timer_load();
    s_timers_loaded = true;
    return true;
}

void timer_load() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < MAX_TIMERS; i++) {
        int addr = EEPROM_TIMER_BASE + i * sizeof(timer_config_t);
        uint8_t *dst = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            dst[j] = EEPROM.read(addr + j);
    }
    EEPROM.end();
}

void timer_save() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < MAX_TIMERS; i++) {
        int addr = EEPROM_TIMER_BASE + i * sizeof(timer_config_t);
        uint8_t *src = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            EEPROM.write(addr + j, src[j]);
    }
    EEPROM.commit();
    EEPROM.end();
}

bool timer_get(int index, timer_config_t *out) {
    if (index < 0 || index >= MAX_TIMERS) return false;
    if (out) *out = s_timers[index];
    return true;
}

bool timer_set(int index, const timer_config_t *cfg) {
    if (index < 0 || index >= MAX_TIMERS || !cfg) return false;
    s_timers[index] = *cfg;
    s_last_fired_minute[index] = 0xFF;
    return true;
}

int8_t timer_check(unsigned long current_epoch, int timezone_offset) {
    if (!s_timers_loaded) return -1;
    if (current_epoch < 100000) return -1; // time not synced

    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday; // 0=Sun
    uint8_t now_minute_id = now_hour * 60 + now_min;

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].enabled) continue;

        uint8_t timer_minute_id = s_timers[i].hour * 60 + s_timers[i].minute;
        if (timer_minute_id != now_minute_id) continue;
        if (s_last_fired_minute[i] == now_minute_id) continue;

        // check days_mask: if 0, run daily; else check bit
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
    *next_epoch = 0;
    *next_action = 0;
    if (!s_timers_loaded || current_epoch < 100000) return;

    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday;
    uint16_t now_slot = now_hour * 60 + now_min;

    unsigned long best_epoch = 0;
    uint8_t best_action = 0;

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].enabled) continue;
        uint16_t timer_slot = s_timers[i].hour * 60 + s_timers[i].minute;

        // check days_mask
        bool day_ok = (s_timers[i].days_mask == 0);
        if (!day_ok) {
            for (int d = 0; d < 7; d++) {
                if (s_timers[i].days_mask & (1 << d)) { day_ok = true; break; }
            }
        }
        if (!day_ok) continue;

        // Simple: find next timer today (or earliest tomorrow)
        unsigned long candidate = current_epoch;
        if (timer_slot <= now_slot) {
            // next occurrence is tomorrow (or later this week)
            candidate += 86400; // next day
        }
        candidate += (timer_slot - now_slot) * 60;
        candidate -= timezone_offset * 3600;

        if (best_epoch == 0 || candidate < best_epoch) {
            best_epoch = candidate;
            best_action = s_timers[i].action;
        }
    }

    *next_epoch = best_epoch;
    *next_action = best_action;
}

void timer_to_json(JsonDocument &doc) {
    JsonArray arr = doc["timers"].to<JsonArray>();
    for (int i = 0; i < MAX_TIMERS; i++) {
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
    if (count > MAX_TIMERS) count = MAX_TIMERS;
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

- [ ] **Step 5: Update pages.h title**

In `clients/esp8266_onoff/include/pages.h`, change line 11:

```c
<title id="pageTitle">OnOff</title>
```

Line 44:

```c
<h1 id="deviceName">OnOff</h1>
```

- [ ] **Step 6: Update platformio.ini**

In `clients/esp8266_onoff/platformio.ini`, change if needed (should be same as lampada).

- [ ] **Step 7: Initial build test**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -10
```
Expected: SUCCESS (or minor compile errors to fix from the copy)

---

### Task 5: Client — ESP-NOW time sync receive

**Files:**
- Modify: `clients/esp8266_onoff/src/main.cpp` — add handler for `ESPNOW_MSG_TIME_SYNC` in `espnow_recv_cb`

**Interfaces:**
- Consumes: `espnow_time_sync_t` from protocol.h
- Produces: `s_time_epoch`, `s_time_synced`, `s_time_millis` global state

- [ ] **Step 1: Add time state variables**

In `clients/esp8266_onoff/src/main.cpp`, add after existing static variables (around line 38-45):

```c
/* Time sync */
static bool s_time_synced = false;
static unsigned long s_time_epoch = 0;
static unsigned long s_time_millis = 0;
```

- [ ] **Step 2: Add time sync header**

Add at top of `clients/esp8266_onoff/src/main.cpp`:

```c
#include <sys/time.h>  /* for settimeofday */
```

- [ ] **Step 3: Add handler in recv_cb**

In `clients/esp8266_onoff/src/main.cpp`, inside `espnow_recv_cb`, add a new case in the switch statement (between ACK and the closing brace):

```c
    case ESPNOW_MSG_TIME_SYNC:
    {
        if (len < sizeof(espnow_time_sync_t))
            return;
        espnow_time_sync_t *ts = (espnow_time_sync_t *)data;
        s_time_epoch = ts->epoch_seconds;
        s_time_millis = millis();
        s_time_synced = true;
        /* Sync system time so localtime() works for timer check */
        struct timeval tv = { .tv_sec = (time_t)s_time_epoch, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        break;
    }
```

- [ ] **Step 3: Build verification**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -10
```
Expected: SUCCESS

---

### Task 6: Client — Timer integration in main loop

**Files:**
- Modify: `clients/esp8266_onoff/src/main.cpp` — add timer init in setup, timer check in loop, EEPROM load/save, include timer.h

- [ ] **Step 1: Add includes**

Add at top of `clients/esp8266_onoff/src/main.cpp`:

```c
#include "timer.h"
```

- [ ] **Step 2: Add timezone variable**

Add after existing static variables:

```c
static int8_t s_timezone_offset = -3;  // default BR timezone
```

- [ ] **Step 3: Get current time helper**

Add a helper function:

```c
static unsigned long get_current_epoch() {
    if (!s_time_synced) return 0;
    return s_time_epoch + (millis() - s_time_millis) / 1000;
}
```

- [ ] **Step 4: Add timer check timer variable**

Add after existing state variables:

```c
static unsigned long s_last_timer_check = 0;
```

- [ ] **Step 5: Init timer in setup**

In `setup()`, after `init_hardware();`:

```c
    timer_init();

    /* Load timezone from EEPROM */
    EEPROM.begin(EEPROM_SIZE);
    int8_t tz = (int8_t)EEPROM.read(EEPROM_TIMEZONE_ADDR);
    if (tz >= -12 && tz <= 14) s_timezone_offset = tz;
    EEPROM.end();
```

- [ ] **Step 6: Add timer check in loop**

In `loop()`, after the LED section (line 1501, before closing `}`), add:

```c
    /* Timer check */
    if (s_time_synced && now - s_last_timer_check > TIMER_CHECK_INTERVAL_MS) {
        s_last_timer_check = now;
        int8_t timer_action = timer_check(get_current_epoch(), s_timezone_offset);
        if (timer_action >= 0) {
            set_relay(timer_action == 1);
            console.printf("[%s] Timer trigger -> relay %s\n", TAG, timer_action ? "ON" : "OFF");
        }
    }
```

- [ ] **Step 7: Build verification**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -10
```
Expected: SUCCESS

---

### Task 7: Client — Timer API endpoints

**Files:**
- Modify: `clients/esp8266_onoff/src/main.cpp` — add `handle_api_timers`, add route in setup, update `/api/state` and `/api/settings`

- [ ] **Step 1: Add /api/timers handler**

Add handler functions before `handle_console`:

```c
static void handle_api_timers(void) {
    if (s_server.method() == HTTP_GET) {
        String json;
        JsonDocument doc;
        timer_to_json(doc);
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    } else if (s_server.method() == HTTP_POST) {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        if (!timer_from_json(doc)) {
            s_server.send(400, "application/json", "{\"error\":\"invalid timers\"}");
            return;
        }
        timer_save();
        String json;
        JsonDocument resp;
        resp["status"] = "ok";
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
        console.printf("[%s] Timers saved\n", TAG);
    }
}
```

- [ ] **Step 2: Register route in setup**

In `setup()`, after `/api/settings` route:

```c
    s_server.on("/api/timers", HTTP_ANY, handle_api_timers);
```

- [ ] **Step 3: Update /api/state**

Add time/timer fields to `handle_api_state`:

```c
        doc["time_synced"] = s_time_synced;
        unsigned long epoch = get_current_epoch();
        unsigned long next_epoch;
        uint8_t next_action;
        timer_get_next(epoch, s_timezone_offset, &next_epoch, &next_action);
        if (next_epoch > 0) {
            doc["next_timer_in"] = (int)(next_epoch - epoch);
            doc["next_timer_action"] = next_action ? "ON" : "OFF";
        } else {
            doc["next_timer_in"] = -1;
            doc["next_timer_action"] = "-";
        }
```

- [ ] **Step 4: Update /api/settings**

In GET handler, add:

```c
        doc["timezone_offset"] = s_timezone_offset;
```

In POST handler, add after the existing `if (doc.containsKey("led_enabled"))` block:

```c
        if (doc.containsKey("timezone_offset")) {
            int8_t new_tz = doc["timezone_offset"] | 0;
            if (new_tz >= -12 && new_tz <= 14 && new_tz != s_timezone_offset) {
                s_timezone_offset = new_tz;
                EEPROM.begin(EEPROM_SIZE);
                EEPROM.write(EEPROM_TIMEZONE_ADDR, (uint8_t)new_tz);
                EEPROM.commit();
                EEPROM.end();
                console.printf("[%s] Timezone offset changed to %d\n", TAG, new_tz);
                changed = true;
            }
        }
```

- [ ] **Step 5: Add /docs entries for timers**

In `clients/esp8266_onoff/include/pages.h`, add doc entries for `/api/timers` endpoint.

- [ ] **Step 6: Build verification**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -10
```
Expected: SUCCESS

---

### Task 8: Client — Dashboard timer UI

**Files:**
- Modify: `clients/esp8266_onoff/include/pages.h` — add timers section to dashboard

- [ ] **Step 1: Add timer section CSS**

In the dashboard CSS section, add after existing style rules:

```css
.timer-section{margin-top:8px}
.timer-row{display:flex;align-items:center;gap:4px;padding:4px 0;border-bottom:1px solid #2a2a4a;font-size:.8em;flex-wrap:wrap}
.timer-row input[type=time]{padding:2px 4px;border-radius:4px;border:1px solid #2a2a4a;background:#1a1a2e;color:#eee;font-size:.8em;width:80px}
.timer-row select{padding:2px 4px;border-radius:4px;border:1px solid #2a2a4a;background:#1a1a2e;color:#eee;font-size:.8em;width:80px}
.timer-row label{color:#888;font-size:.8em;display:flex;align-items:center;gap:2px}
.timer-row input[type=checkbox]{margin:0}
.days-group{display:flex;gap:2px}
.days-group label{font-size:.7em;color:#888}
```

- [ ] **Step 2: Add timer HTML section**

In the HTML body, after the settings details `</div>` and before the footer `</div>`:

```html
<div class="expand-header" onclick="toggleTimers()"><span>Timers</span><span id="expandIcon3">&#9654;</span></div>
<div class="details" id="timerDetails">
<div id="timerList"></div>
<div style="text-align:center;margin-top:8px"><button class="save-btn" onclick="saveTimers()">Salvar Timers</button></div>
</div>
```

- [ ] **Step 3: Add timer JS**

Add timer-related JS functions and variables alongside existing JS. Add after existing variable declarations:

```js
const timerDetails=document.getElementById('timerDetails');
const expandIcon3=document.getElementById('expandIcon3');
const timerList=document.getElementById('timerList');
let expanded3=false;
function toggleTimers(){expanded3=!expanded3;timerDetails.classList.toggle('open',expanded3);expandIcon3.classList.toggle('open',expanded3)}
async function saveTimers(){
  let timers=[];
  for(let i=0;i<6;i++){
    let chk=document.getElementById('timer_en_'+i);
    let tm=document.getElementById('timer_time_'+i).value.split(':');
    let act=document.getElementById('timer_act_'+i).value;
    let mask=0;
    for(let d=0;d<7;d++){if(document.getElementById('timer_day_'+i+'_'+d).checked)mask|=1<<d}
    timers.push({hour:parseInt(tm[0]),minute:parseInt(tm[1]),action:parseInt(act),days_mask:mask,enabled:chk.checked});
  }
  try{let r=await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({timers})});
  let d=await r.json();footerEl.textContent='Timers salvos!'}catch(e){footerEl.textContent='Erro: '+e.message}
}
async function loadTimers(){
  try{let r=await fetch('/api/timers');let d=await r.json();
  timerList.innerHTML='';
  d.timers.forEach(function(t,i){
    let div=document.createElement('div');div.className='timer-row';
    div.innerHTML='<label><input type="checkbox" id="timer_en_'+i+'"'+(t.enabled?' checked':'')+'></label>'+
      '<input type="time" id="timer_time_'+i+'" value="'+(t.hour+'').padStart(2,'0')+':'+(t.minute+'').padStart(2,'0')+'">'+
      '<select id="timer_act_'+i+'"><option value="0"'+(t.action===0?' selected':'')+'>OFF</option><option value="1"'+(t.action===1?' selected':'')+'>ON</option></select>'+
      '<div class="days-group">'+
        ['D','S','T','Q','Q','S','S'].map((l,d)=>'<label><input type="checkbox" id="timer_day_'+i+'_'+d+'"'+(t.days_mask&(1<<d)?' checked':'')+'>'+l+'</label>').join('')+
      '</div>';
    timerList.appendChild(div)})}catch(e){}
}
```

- [ ] **Step 4: Update fetchState for timer info**

Add to the footer section update in `fetchState()` (replace the footer line):

```js
      let nextInfo='';
      if(d.time_synced){
        if(d.next_timer_in>=0)nextInfo=' Prox timer: '+d.next_timer_action+' em '+d.next_timer_in+'s';
        else nextInfo=' Sem timer ativo';
      } else {nextInfo=' Aguardando hora...'}
      footerEl.textContent=d.device_id+(d.last_send_s?' Ultimo envio: '+d.last_send_s+'s ago':'')+nextInfo;
```

- [ ] **Step 5: Load timers on page load**

Add `loadTimers();` alongside the existing `fetchState(); fetchSettings();`:

```js
setInterval(fetchState,3000);fetchState();fetchSettings();loadTimers();
```

- [ ] **Step 6: Build verification**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -10
```
Expected: SUCCESS

---

### Task 9: Client — initial docs

**Files:**
- Modify: `clients/esp8266_onoff/SPEC.md` — add timer specification
- Modify: `clients/esp8266_onoff/README.md` — update for onoff device

- [ ] **Step 1: Update SPEC.md**

Change title to "ESP8266 OnOff (Timer Switch Client)", update all references from "lampada" to "onoff", add timer section:

```markdown
## Timer (Schedule)

- 6 timer slots configurable via dashboard
- Each timer: hour, minute, action (ON/OFF), days of week mask
- Time sync via ESP-NOW from gateway (no NTP on client)
- Timer check every 10s in non-blocking loop
- EEPROM persisted
```

- [ ] **Step 2: Update README.md**

Change title, description, pin table. Keep hardware section similar (GPIOs same as lampada).

- [ ] **Step 3: Build verification**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff
pio run 2>&1 | tail -5
```
Expected: SUCCESS

---

### Task 10: Full build verification

- [ ] **Step 1: Build gateway**

```bash
cd /mnt/c/project/homeware/gateway && pio run 2>&1 | tail -5
```
Expected: SUCCESS

- [ ] **Step 2: Build onoff client**

```bash
cd /mnt/c/project/homeware/clients/esp8266_onoff && pio run 2>&1 | tail -5
```
Expected: SUCCESS

- [ ] **Step 3: Build existing lampada (ensure no regression)**

```bash
cd /mnt/c/project/homeware/clients/esp8266_lampada && pio run 2>&1 | tail -5
```
Expected: SUCCESS
