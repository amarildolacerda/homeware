# ESP8266 OnOff Client — Design Spec

## Overview

Create a new `esp8266_onoff` client by forking `esp8266_lampada`, adding a programmable timer feature that syncs time via ESP-NOW from the gateway. The gateway is extended to broadcast NTP-synchronized time periodically.

## Motivation

The lampada client works as a relay switch but lacks timer/scheduling features. A dedicated onoff client with programmable timer enables use cases like "turn off after 30min" or "turn on at sunset" without relying on external HA automations. For clients far from WiFi, time sync must work over ESP-NOW only.

## 1. esp8266_onoff — New Client

### 1.1 Source
- Copied from `clients/esp8266_lampada` → `clients/esp8266_onoff`
- Same sensor type: `SENSOR_TYPE_ONOFF`
- Same ESP-NOW protocol, relay control, WiFiManager, Alexa, OTA, LED, button, telnet
- No changes to lampada

### 1.2 New Files

```
clients/esp8266_onoff/
├── include/
│   ├── timer.h             ← Timer declarations
│   └── pages.h             ← Updated dashboard + docs
├── src/
│   └── timer.cpp           ← Timer logic
```

### 1.3 Timer Module (timer.h/.cpp)

**Config (6 slots):**
```c
#define MAX_TIMERS 6

typedef struct {
    uint8_t hour;        // 0-23
    uint8_t minute;      // 0-59
    uint8_t action;      // 0=OFF, 1=ON
    uint8_t days_mask;   // bit0=Sun...bit6=Sat, 0=all days
    bool enabled;
} timer_config_t;
```

**Time Source (no NTP on client):**
- Receives `ESPNOW_MSG_TIME_SYNC` from gateway (epoch_seconds + local millis snapshot)
- Maintains `s_time_epoch` updated by: last_sync_epoch + (millis() - last_sync_millis) / 1000
- Falls back to last known time across reboots? No — time starts at 0 until first sync
- Dashboard shows `time_synced: true/false`

**Timer Execution:**
- Check every ~10s in loop (compare HH:MM, days_mask)
- On match: call `set_relay(action)` if state differs
- Debounce: mark `s_timer_fired_minute` to prevent repeat within same minute
- On relay manual toggle: clear `s_timer_fired_minute`

### 1.4 Dashboard Updates (pages.h)

Same compact layout as lampada + new collapsible "Timers" section:

```
Timers                 [▶ expand]
├─ Timer 1: ☑ [12:00] [Ligar]  □D □S □T □Q □Q □S □S
├─ Timer 2: ☑ [18:30] [Desligar] □D □S □T □Q □Q □S □S
├─ ... até 6
└─ [💾 Salvar Timers]
```

### 1.5 API Endpoints (new)

| Route | Method | Description |
|-------|--------|-------------|
| `GET /api/timers` | GET | Return JSON array of timer configs |
| `POST /api/timers` | POST | Save timer config array to EEPROM |

### 1.6 API Endpoints (modified)

| Route | Change |
|-------|--------|
| `GET /api/state` | Add `time_synced` (bool), `next_timer_in` (seconds), `next_timer_action` (string) |
| `GET /api/settings` | Add `timezone_offset` (int, hours) |
| `POST /api/settings` | Accept `timezone_offset` |

### 1.7 EEPROM Layout (forked from lampada)

```
EEPROM_GATEWAY_MAC_ADDR  = 0   (6B magic+mac)
EEPROM_NAME_ADDR         = 10  (48B)
EEPROM_RELAY_STATE_ADDR  = 59  (1B)
EEPROM_RELAY_PIN_ADDR    = 60  (1B)
EEPROM_BUTTON_PIN_ADDR   = 61  (1B)
EEPROM_LED_ENABLED_ADDR  = 62  (1B)
EEPROM_STARTUP_MODE_ADDR = 63  (1B)
EEPROM_SSID_ADDR         = 64  (32B)
EEPROM_PASS_ADDR         = 96  (64B)
--- NEW ---
EEPROM_TIMEZONE_ADDR     = 160 (1B, int8_t, -12..+14)
EEPROM_TIMER_BASE        = 161 (5B × 6 = 30B)
```
Total: 191 of 256 bytes.

### 1.8 ESP-NOW Receive Handler

Add handler for `ESPNOW_MSG_TIME_SYNC`:

```c
case ESPNOW_MSG_TIME_SYNC:
    espnow_time_sync_t *ts = (espnow_time_sync_t*)data;
    s_time_epoch = ts->epoch_seconds;
    s_time_millis = millis();
    s_time_synced = true;
    break;
```

## 2. Gateway — NTP + Time Sync Broadcast

### 2.1 NTP Initialization

In `main.cpp` setup, after WiFi connected:
```c
configTime(0, 0, "pool.ntp.org", "time.nist.gov");
// Non-blocking: flag s_ntp_synced checked in loop
```

### 2.2 New Config Constants

In `config.h`:
```c
#define TIME_SYNC_INTERVAL_MS 300000  // 5 minutes
#define NTP_RETRY_INTERVAL_MS 1800000 // 30 minutes
```

### 2.3 Time Data

In `main.cpp` or `espnow_handler.cpp`:
```c
static bool s_ntp_synced = false;
static unsigned long s_last_time_sync = 0;
static unsigned long s_last_ntp_retry = 0;
static time_t s_ntp_epoch = 0;
```

### 2.4 ESP-NOW Time Broadcast

In gateway's `loop()` (or `espnow_handler_loop()`):
```c
if (s_ntp_synced && millis() - s_last_time_sync > TIME_SYNC_INTERVAL_MS) {
    s_last_time_sync = millis();
    // Broadcast TIME_SYNC to all
    espnow_time_sync_t ts = {0};
    ts.msg_type = ESPNOW_MSG_TIME_SYNC;
    ts.sequence = s_time_sync_sequence++;
    mac_copy(ts.gateway_mac, s_gateway_mac);
    ts.epoch_seconds = (uint32_t)s_ntp_epoch;
    ts.epoch_millis = 0;
    esp_now_send(s_broadcast_mac, (uint8_t*)&ts, sizeof(ts));
}
```

The broadcast MAC is `{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}` (same as used by clients for pair requests).

### 2.5 NTP Retry

If NTP not yet synced, retry every `NTP_RETRY_INTERVAL_MS`:
```c
if (!s_ntp_synced && millis() - s_last_ntp_retry > NTP_RETRY_INTERVAL_MS) {
    s_last_ntp_retry = millis();
    s_ntp_epoch = time(nullptr);
    if (s_ntp_epoch > 100000) s_ntp_synced = true;
}
```

### 2.6 Updated Console Commands

- `s` (status): add line `NTP: synced / not synced`
- No new commands needed

### 2.7 Updated Web API

- `GET /api/info`: add `ntp_synced` field

## 3. Protocol Changes

### 3.1 New Message Type

In `gateway/include/espnow_protocol.h` and `clients/esp8266_onoff/include/espnow_protocol.h`:

```c
typedef enum {
    ESPNOW_MSG_SENSOR_DATA = 0x01,
    ESPNOW_MSG_PAIR_REQUEST = 0x02,
    ESPNOW_MSG_PAIR_RESPONSE = 0x03,
    ESPNOW_MSG_ACK = 0x04,
    ESPNOW_MSG_HEARTBEAT = 0x05,
    ESPNOW_MSG_OTA_TRIGGER = 0x06,
    ESPNOW_MSG_COMMAND = 0x07,
    ESPNOW_MSG_TIME_SYNC = 0x08,
} espnow_msg_type_t;
```

### 3.2 New Struct

```c
typedef struct __attribute__((packed)) {
    uint8_t msg_type;           // ESPNOW_MSG_TIME_SYNC
    uint16_t sequence;
    uint8_t gateway_mac[6];
    uint32_t epoch_seconds;
    uint16_t epoch_millis;
} espnow_time_sync_t;
```

### 3.3 Backward Compatibility

Clients that don't know `ESPNOW_MSG_TIME_SYNC` (existing lampada, chuva, etc.) will hit the `default: s_crc_errors++;` case in their receive handler. This is acceptable — they'll add an insignificant CRC error count. The gateway's broadcast is 1 packet every 5 minutes.

## 4. Files Changed

### Gateway (stable — minimal changes per user request):
- `gateway/include/espnow_protocol.h` — new msg type + struct
- `gateway/include/espnow_handler.h` — new function declarations
- `gateway/include/config.h` — new constants
- `gateway/src/main.cpp` — NTP init + time sync loop
- `gateway/src/espnow_handler.cpp` — time sync broadcast function
- `gateway/include/platform.h` — maybe add `s_broadcast_mac` if not present

### New Client (esp8266_onoff):
- `clients/esp8266_onoff/` (entire directory, forked from lampada)
- `clients/esp8266_onoff/include/espnow_protocol.h` — same as gateway
- `clients/esp8266_onoff/include/timer.h` — new
- `clients/esp8266_onoff/src/timer.cpp` — new
- `clients/esp8266_onoff/src/main.cpp` — add timer check + time sync receive
- `clients/esp8266_onoff/include/pages.h` — add timer UI
- `clients/esp8266_onoff/include/config.h` — updated

### Protocol (shared struct — update both copies):
- `gateway/include/espnow_protocol.h`
- `clients/esp8266_onoff/include/espnow_protocol.h`

### Other clients — NOT modified:
- `clients/esp8266_lampada/` — untouched
- `clients/esp8266_chuva/` — untouched
- `clients/esp8266_dht_gas/` — untouched
- `clients/esp8266_repeater/` — untouched
