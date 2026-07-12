# Repeater Status Reporting — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the repeater send its stats (RX/TX, clients, radio, ACK failures) to the gateway every 30s via ESP-NOW, display it on the gateway dashboard.

**Architecture:** New ESP-NOW message type `REPEATER_STATUS (0x0B)` with `SENSOR_TYPE_REPEATER (9)`. Repeater sends every 30s to gateway. Gateway registers repeater in sensor_registry, exposes via `/api/sensors`, dashboard shows dedicated section.

**Tech Stack:** ESP8266 Arduino, ESP-NOW, ArduinoJson, ESP8266WebServer

## Global Constraints

- ESP-NOW frame max 250 bytes; payload is 16 bytes
- Both protocol headers (gateway + repeater) must stay in sync — same enum values, same structs
- Non-blocking: timer uses `millis()`, no `delay()` in send path
- Repeater does NOT pair like a sensor — gateway auto-registers on first receive
- No EEPROM layout change — repeater status is transient (RAM only)
- FW_VERSION = v0.0.22

## File Map

| File | Change |
|------|--------|
| `gateway/include/espnow_protocol.h` | Add `0x0B`, `SENSOR_TYPE_REPEATER=9`, `payload_repeater_status_t` |
| `gateway/include/sensor_registry.h` | Add `repeater` union member |
| `gateway/src/espnow_handler.cpp` | Add case `0x0B` in recv_cb |
| `gateway/src/sensor_registry.cpp` | Add case in `update_state()` + `sensor_type_to_string()` |
| `gateway/src/web_server.cpp` | Add REPEATER case in `/api/sensors` |
| `gateway/include/pages.h` | Add repeater section to dashboard |
| `clients/esp8266_repeater/include/espnow_protocol.h` | Add `0x0B`, `SENSOR_TYPE_REPEATER=9`, `payload_repeater_status_t` |
| `clients/esp8266_repeater/src/main.cpp` | Add 30s timer + `send_repeater_status()` |

---

### Task 1: Update ESP-NOW protocol headers (both gateway + repeater)

**Files:**
- Modify: `gateway/include/espnow_protocol.h`
- Modify: `clients/esp8266_repeater/include/espnow_protocol.h`

**Interfaces:**
- Consumes: Nothing (standalone)
- Produces: `ESPNOW_MSG_REPEATER_STATUS = 0x0B`, `SENSOR_TYPE_REPEATER = 9`, `payload_repeater_status_t`

- [ ] **Step 1: Add message type and sensor type to gateway protocol**

In `gateway/include/espnow_protocol.h`, add to the `espnow_msg_type_t` enum (after `ESPNOW_MSG_GW_DISCOVER`):
```c
ESPNOW_MSG_REPEATER_STATUS = 0x0B,
```

Add to the `sensor_type_t` enum (after `SENSOR_TYPE_LIGHT`):
```c
SENSOR_TYPE_REPEATER = 9,
```

Add the payload struct (after `payload_onoff_t`):
```c
typedef struct __attribute__((packed)) {
    uint16_t received;
    uint16_t forwarded;
    uint8_t  client_count;
    uint8_t  channel;
    int16_t  rssi;
    uint32_t uptime_s;
    uint16_t free_heap;
    uint8_t  ack_failures;
} payload_repeater_status_t;
```

- [ ] **Step 2: Copy same changes to repeater protocol**

In `clients/esp8266_repeater/include/espnow_protocol.h`, add the exact same three additions (enum value, sensor type, payload struct).

- [ ] **Step 3: Build both to verify**

```bash
pio run -d gateway 2>&1 | tail -5
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

Expected: both SUCCESS

- [ ] **Step 4: Commit**

```bash
git add gateway/include/espnow_protocol.h clients/esp8266_repeater/include/espnow_protocol.h
git commit -m "feat(protocol): add REPEATER_STATUS msg type and SENSOR_TYPE_REPEATER"
```

---

### Task 2: Add repeater union member to gateway sensor_registry

**Files:**
- Modify: `gateway/include/sensor_registry.h`
- Modify: `gateway/src/sensor_registry.cpp`

**Interfaces:**
- Consumes: `payload_repeater_status_t` from Task 1
- Produces: `virtual_sensor_t.state.repeater` fields, `update_state()` case for REPEATER

- [ ] **Step 1: Add repeater struct to union in sensor_registry.h**

In `gateway/include/sensor_registry.h`, add to the `state` union (after the `onoff` member):
```c
struct {
    uint16_t received;
    uint16_t forwarded;
    uint8_t  client_count;
    uint8_t  channel;
    uint32_t uptime_s;
    uint16_t free_heap;
    uint8_t  ack_failures;
} repeater;
```

- [ ] **Step 2: Add update_state case in sensor_registry.cpp**

In `gateway/src/sensor_registry.cpp`, in the `sensor_registry_update_state()` function, add a case in the first switch block (after `SENSOR_TYPE_ONOFF`/`SENSOR_TYPE_LIGHT`):
```c
case SENSOR_TYPE_REPEATER: {
    if (payload_len >= sizeof(payload_repeater_status_t)) {
        payload_repeater_status_t *p = (payload_repeater_status_t*)payload;
        s->state.repeater.received = p->received;
        s->state.repeater.forwarded = p->forwarded;
        s->state.repeater.client_count = p->client_count;
        s->state.repeater.channel = p->channel;
        s->state.repeater.uptime_s = p->uptime_s;
        s->state.repeater.free_heap = p->free_heap;
        s->state.repeater.ack_failures = p->ack_failures;
    }
    break;
}
```

Add to the second switch block (the `expected` size calculation, after `SENSOR_TYPE_ONOFF`/`SENSOR_TYPE_LIGHT`):
```c
case SENSOR_TYPE_REPEATER: expected = sizeof(payload_repeater_status_t); break;
```

- [ ] **Step 3: Add to sensor_type_to_string()**

In `gateway/src/sensor_registry.cpp`, find `sensor_type_to_string()` and add:
```c
case SENSOR_TYPE_REPEATER: return "repeater";
```

Also add to `sensor_type_friendly_name()`:
```c
case SENSOR_TYPE_REPEATER: return "Repeater";
```

- [ ] **Step 4: Build gateway**

```bash
pio run -d gateway 2>&1 | tail -5
```

Expected: SUCCESS

- [ ] **Step 5: Commit**

```bash
git add gateway/include/sensor_registry.h gateway/src/sensor_registry.cpp
git commit -m "feat(gateway): add SENSOR_TYPE_REPEATER to sensor registry"
```

---

### Task 3: Handle REPEATER_STATUS in gateway espnow_handler

**Files:**
- Modify: `gateway/src/espnow_handler.cpp`

**Interfaces:**
- Consumes: `payload_repeater_status_t` from Task 1, sensor_registry from Task 2
- Produces: Processes incoming REPEATER_STATUS messages, sends ACK

- [ ] **Step 1: Add case in espnow_recv_cb()**

In `gateway/src/espnow_handler.cpp`, in the `espnow_recv_cb()` switch block, add a new case after `ESPNOW_MSG_GW_DISCOVER`:
```c
case ESPNOW_MSG_REPEATER_STATUS: {
    if (len < ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t)) { s_crc_errors++; return; }
    espnow_header_t *hdr = (espnow_header_t*)data;

    if (hdr->version != ESPNOW_PROTOCOL_VERSION) { s_crc_errors++; return; }

    int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
    {
        char sender_str[18], sensor_str[18];
        mac_to_str(mac, sender_str, sizeof(sender_str));
        mac_to_str(hdr->sensor_mac, sensor_str, sizeof(sensor_str));
        console.printf("[ESP-NOW] REPEATER_STATUS: sender=%s sensor=%s slot=%d\n",
            sender_str, sensor_str, slot);
    }

    if (slot < 0) {
        slot = sensor_registry_find_free_slot();
        if (slot < 0) {
            console.printf("[ESP-NOW] Repeater rejected: registry full\n");
            send_ack(mac, hdr->sequence, PAIR_STATUS_FULL, 0xFF);
            return;
        }
        sensor_registry_add(hdr->sensor_mac, hdr->sensor_type, slot, "Repeater");
    }

    sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
    send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
    log_add("info", "Repeater status slot %d seq %d", slot, hdr->sequence);
    s_ack_count++;
    queue_bridge_state(slot);
    break;
}
```

- [ ] **Step 2: Build gateway**

```bash
pio run -d gateway 2>&1 | tail -5
```

Expected: SUCCESS

- [ ] **Step 3: Commit**

```bash
git add gateway/src/espnow_handler.cpp
git commit -m "feat(gateway): handle REPEATER_STATUS in espnow recv callback"
```

---

### Task 4: Add REPEATER to gateway /api/sensors

**Files:**
- Modify: `gateway/src/web_server.cpp`

**Interfaces:**
- Consumes: `virtual_sensor_t.state.repeater` from Task 2
- Produces: Repeater fields in `/api/sensors` JSON response

- [ ] **Step 1: Add REPEATER case in /api/sensors switch**

In `gateway/src/web_server.cpp`, in the `/api/sensors` endpoint handler, add a case in the state switch block (after `SENSOR_TYPE_ONOFF`/`SENSOR_TYPE_LIGHT`):
```c
case SENSOR_TYPE_REPEATER:
    state["received"] = s->state.repeater.received;
    state["forwarded"] = s->state.repeater.forwarded;
    state["client_count"] = s->state.repeater.client_count;
    state["channel"] = s->state.repeater.channel;
    state["uptime_s"] = s->state.repeater.uptime_s;
    state["free_heap"] = s->state.repeater.free_heap;
    state["ack_failures"] = s->state.repeater.ack_failures;
    break;
```

- [ ] **Step 2: Build gateway**

```bash
pio run -d gateway 2>&1 | tail -5
```

Expected: SUCCESS

- [ ] **Step 3: Commit**

```bash
git add gateway/src/web_server.cpp
git commit -m "feat(gateway): add REPEATER fields to /api/sensors"
```

---

### Task 5: Add repeater section to gateway dashboard

**Files:**
- Modify: `gateway/include/pages.h`

**Interfaces:**
- Consumes: `/api/sensors` JSON with repeater type from Task 4
- Produces: Repeater section in gateway dashboard HTML/JS

- [ ] **Step 1: Add repeater section to overview page**

In `gateway/include/pages.h`, find the overview page content (the `PAGE_OVERVIEW` constant). Add a new section after the sensor cards for repeaters:

```html
<div id="repeaterSection" style="display:none">
<h2 style="font-size:0.95rem;font-weight:600;color:var(--primary);margin:20px 0 12px">Repeaters</h2>
<div id="repeaterList"></div>
</div>
```

- [ ] **Step 2: Add JS to render repeaters**

In the JS section of the overview page, add a function to render repeater cards and call it from the existing `loadSensors()` function:

```js
function renderRepeaters(sensors) {
    const list = document.getElementById('repeaterList');
    const section = document.getElementById('repeaterSection');
    const repeaters = sensors.filter(s => s.type_name === 'repeater');
    if (!repeaters.length) { section.style.display = 'none'; return; }
    section.style.display = '';
    list.innerHTML = repeaters.map(r => {
        const st = r.state || {};
        const ago = r.last_seen >= 0 ? fmtDuration(r.last_seen) : 'nunca';
        return `<div class="card" style="margin-bottom:12px">
            <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px">
                <div>
                    <span style="font-weight:600">${r.name||'Repeater'}</span>
                    <span style="color:var(--muted-subtle);font-size:0.78rem;margin-left:8px">${r.mac}</span>
                </div>
                <span class="badge ${r.online?'on':'off'}">${r.online?'Online':'Offline'}</span>
            </div>
            <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px">
                <div class="metric"><div class="metric-label">Recebidos</div><div class="metric-value">${st.received||0}</div></div>
                <div class="metric"><div class="metric-label">Encaminhados</div><div class="metric-value">${st.forwarded||0}</div></div>
                <div class="metric"><div class="metric-label">Clients</div><div class="metric-value">${st.client_count||0}</div></div>
                <div class="metric"><div class="metric-label">Falhas ACK</div><div class="metric-value" style="color:${st.ack_failures>0?'var(--danger)':'var(--text)'}">${st.ack_failures||0}</div></div>
                <div class="metric"><div class="metric-label">Canal</div><div class="metric-value">${st.channel||'-'}</div></div>
                <div class="metric"><div class="metric-label">RSSI</div><div class="metric-value">${r.last_rssi} dBm</div></div>
                <div class="metric"><div class="metric-label">Uptime</div><div class="metric-value">${fmtDuration(st.uptime_s*1000)}</div></div>
                <div class="metric"><div class="metric-label">Memória</div><div class="metric-value">${fmtHeap(st.free_heap)}</div></div>
            </div>
            <div style="font-size:0.75rem;color:var(--muted-subtle);margin-top:8px">Último envio: ${ago}</div>
        </div>`;
    }).join('');
}
```

Call `renderRepeaters(data)` at the end of the existing `loadSensors()` callback.

- [ ] **Step 3: Add helper functions if missing**

Check if `fmtDuration()` and `fmtHeap()` exist in the existing JS. If not, add:
```js
function fmtDuration(ms) {
    if (ms < 0) return '-';
    const s = Math.floor(ms/1000);
    const d = Math.floor(s/86400); const h = Math.floor(s%86400/3600); const m = Math.floor(s%3600/60);
    return d > 0 ? `${d}d ${h}h` : h > 0 ? `${h}h ${m}m` : `${m}m`;
}
function fmtHeap(b) {
    return b > 1024 ? (b/1024).toFixed(1)+'KB' : b+'B';
}
```

- [ ] **Step 4: Build gateway**

```bash
pio run -d gateway 2>&1 | tail -5
```

Expected: SUCCESS

- [ ] **Step 5: Commit**

```bash
git add gateway/include/pages.h
git commit -m "feat(gateway): add repeater section to dashboard"
```

---

### Task 6: Add status sending to repeater

**Files:**
- Modify: `clients/esp8266_repeater/src/main.cpp`

**Interfaces:**
- Consumes: `payload_repeater_status_t` from Task 1, `s_gateway_mac` and ESP-NOW send from existing code
- Produces: `send_repeater_status()` function, 30s timer in loop()

- [ ] **Step 1: Add send_repeater_status() function**

In `clients/esp8266_repeater/src/main.cpp`, add before `loop()`:

```c
static uint16_t s_status_sequence = 0;

static void send_repeater_status(void) {
    if (!s_gateway_configured) return;

    espnow_header_t hdr = {
        .version = ESPNOW_PROTOCOL_VERSION,
        .msg_type = ESPNOW_MSG_REPEATER_STATUS,
        .sequence = ++s_status_sequence,
        .sensor_type = SENSOR_TYPE_REPEATER,
        .battery_pct = 0,
        .rssi = WiFi.RSSI(),
    };
    /* Set sensor_mac to own MAC */
    uint8_t mac[6];
    WiFi.macAddress(mac);
    memcpy(hdr.sensor_mac, mac, 6);

    payload_repeater_status_t status = {
        .received = (uint16_t)s_received,
        .forwarded = (uint16_t)s_forwarded,
        .client_count = (uint8_t)s_client_count,
        .channel = (uint8_t)WiFi.channel(),
        .rssi = WiFi.RSSI(),
        .uptime_s = (uint32_t)((millis() - s_start_time) / 1000),
        .free_heap = (uint16_t)ESP.getFreeHeap(),
        .ack_failures = 0,
    };

    /* Build frame: header (without payload array) + payload */
    uint8_t frame[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t)];
    memcpy(frame, &hdr, ESPNOW_HEADER_FIXED_SIZE);
    memcpy(frame + ESPNOW_HEADER_FIXED_SIZE, &status, sizeof(status));

    int ret = esp_now_send(s_gateway_mac, frame, sizeof(frame));
    if (ret != 0) {
        console.printf("[%s] REPEATER_STATUS send failed ret=%d\n", TAG, ret);
    }
}
```

- [ ] **Step 2: Add 30s timer in loop()**

In `loop()`, add after the existing gateway timeout check (before the gateway discovery block):

```c
/* Send status to gateway every 30s */
static unsigned long last_status_send = 0;
if (s_gateway_configured && (now - last_status_send > 30000)) {
    last_status_send = now;
    send_repeater_status();
}
```

- [ ] **Step 3: Build repeater**

```bash
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

Expected: SUCCESS

- [ ] **Step 4: Commit**

```bash
git add clients/esp8266_repeater/src/main.cpp
git commit -m "feat(repeater): send REPEATER_STATUS every 30s to gateway"
```

---

### Task 7: Final verification

- [ ] **Step 1: Build gateway and repeater**

```bash
pio run -d gateway 2>&1 | tail -5
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

Expected: both SUCCESS

- [ ] **Step 2: Verify protocol headers are in sync**

```bash
diff <(grep -E "ESPNOW_MSG_REPEATER_STATUS|SENSOR_TYPE_REPEATER|payload_repeater_status_t" gateway/include/espnow_protocol.h) \
     <(grep -E "ESPNOW_MSG_REPEATER_STATUS|SENSOR_TYPE_REPEATER|payload_repeater_status_t" clients/esp8266_repeater/include/espnow_protocol.h)
```

Expected: no output (identical)

- [ ] **Step 3: Verify REPEATER appears in /api/sensors**

Check web_server.cpp has the REPEATER case:
```bash
grep -c "SENSOR_TYPE_REPEATER" gateway/src/web_server.cpp
```

Expected: 1

- [ ] **Step 4: Commit if any fixes needed**

```bash
git add -A && git commit -m "fix: final adjustments after repeater status reporting"
```
