# ESP8266 Clients Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign dashboards of chuva, dht_gas, repeater clients with sidebar layout, stats-header, footer bar, /docs endpoint, and console module extraction — matching the onoff/lampada pattern.

**Architecture:** Each client is fully independent — no shared code across clients. The console module (console.h/cpp) is copied verbatim from onoff. The pages.h is rewritten per client with client-specific hero/state rendering but shared sidebar/footer structure.

**Tech Stack:** ESP8266 Arduino, ESP8266WebServer, ArduinoJson, WiFiManager

## Global Constraints

- Dashboard must use sidebar layout (180px desktop, 48px mobile collapse) matching onoff/lampada
- Stats-header shows RX · TX · Mem + 1 client-specific stat
- Footer shows dot + gateway status · time · uptime (time via `new Date().toLocaleTimeString` JS)
- `/docs` page must list all API endpoints in swagger-like card layout
- `pages.h` must stay under 8KB to fit `send_P()` without chunking
- Console module files (console.h/cpp) must match onoff's API exactly (`printf`, `loop`, `telnet_available`, `telnet_read`, `begin`)
- No changes to sensor logic, ESP-NOW protocol, pairing, EEPROM, or WiFiManager behavior
- FW_VERSION stays at v0.0.21 (no bump needed)

---

### Task 1: Create console module for chuva, dht_gas, repeater

**Files:**
- Create: `clients/esp8266_chuva/include/console.h` (copy from onoff)
- Create: `clients/esp8266_chuva/src/console.cpp` (copy from onoff)
- Create: `clients/esp8266_dht_gas/include/console.h` (copy from onoff)
- Create: `clients/esp8266_dht_gas/src/console.cpp` (copy from onoff)
- Create: `clients/esp8266_repeater/include/console.h` (copy from onoff)
- Create: `clients/esp8266_repeater/src/console.cpp` (copy from onoff)

**Interfaces:**
- Consumes: Nothing (standalone module)
- Produces: `console.h`/`console.cpp` with `Console` class exposing `begin()`, `loop()`, `printf()`, `telnet_available()`, `telnet_read()`

- [ ] **Step 1: Copy console.h and console.cpp to chuva**

```bash
cp /mnt/c/project/homeware/clients/esp8266_onoff/include/console.h /mnt/c/project/homeware/clients/esp8266_chuva/include/console.h
cp /mnt/c/project/homeware/clients/esp8266_onoff/src/console.cpp /mnt/c/project/homeware/clients/esp8266_chuva/src/console.cpp
```

- [ ] **Step 2: Copy console.h and console.cpp to dht_gas**

```bash
cp /mnt/c/project/homeware/clients/esp8266_onoff/include/console.h /mnt/c/project/homeware/clients/esp8266_dht_gas/include/console.h
cp /mnt/c/project/homeware/clients/esp8266_onoff/src/console.cpp /mnt/c/project/homeware/clients/esp8266_dht_gas/src/console.cpp
```

- [ ] **Step 3: Copy console.h and console.cpp to repeater**

```bash
cp /mnt/c/project/homeware/clients/esp8266_onoff/include/console.h /mnt/c/project/homeware/clients/esp8266_repeater/include/console.h
cp /mnt/c/project/homeware/clients/esp8266_onoff/src/console.cpp /mnt/c/project/homeware/clients/esp8266_repeater/src/console.cpp
```

- [ ] **Step 4: Commit**

```bash
git add clients/esp8266_chuva/include/console.h clients/esp8266_chuva/src/console.cpp clients/esp8266_dht_gas/include/console.h clients/esp8266_dht_gas/src/console.cpp clients/esp8266_repeater/include/console.h clients/esp8266_repeater/src/console.cpp
git commit -m "feat(chuva,dht_gas,repeater): add console module"
```

---

### Task 2: Redesign chuva dashboard + main.cpp

**Files:**
- Rewrite: `clients/esp8266_chuva/include/pages.h`
- Modify: `clients/esp8266_chuva/src/main.cpp`

**Interfaces:**
- Consumes: Console module from Task 1
- Produces: New dashboard HTML at `/`, docs at `/docs`, settings API at `/api/settings`

- [ ] **Step 1: Rewrite pages.h**

Write `clients/esp8266_chuva/include/pages.h` with:
- Sidebar: Home · Propriedades · Configurações
- Stats-header: RX · TX · Mem · Nível (rain_level)
- Hero: large rain % + color-coded badge (SECO/CHUVISCO/CHUVENDO/CHUVA FORTE)
- Collapsible "Detalhes" section: Digital, Bateria, RSSI, IP, Versão FW, Slot, Gateway
- Footer: dot + gateway status · time · uptime
- `/docs` page with endpoints card layout
- WiFi config page (same as onoff)
- Polling 3s via `setInterval`
- Keep `PAGE_DASHBOARD`, `PAGE_DOCS`, `PAGE_WIFI_CONFIG` constants

The dashboard section of pages.h should reference these JS variables for state:
```
rainLevel div       → d.rain_level + "%"
rainBadge span      → SECO/CHUVISCO/CHUVENDO/CHUVA FORTE based on d.rain_level
rainDigital span    → d.rain_digital ? "Molhado" : "Seco"
battery span        → d.battery + "%"
rssi span           → d.rssi + " dBm"
ip span             → d.ip
fwVersion span      → d.fw_version
slot span           → d.slot
gatewayStatus span  → d.gateway_connected ? "Online" : "Offline"
rxVal, txVal, memVal → d.rx_count, d.tx_count, d.free_heap
levelVal            → d.rain_level (for stats-header Nível stat)
deviceName input    → d.device_name
```

Use the same CSS variables (`--bg`, `--surface`, `--primary`, `--border`, etc) as onoff. The dashboard is on the `#secHome` section. Properties on `#secPropriedades`. Config on `#secConfig`. The hero should be centered with a large circular display of the rain level percentage.

- [ ] **Step 2: Add console includes and serial handling in main.cpp**

In `clients/esp8266_chuva/src/main.cpp`:
- Add `#include "console.h"` after existing includes
- Replace inline `handle_serial()` function calls with `console.begin()`, `console.loop()`, and `handle_console(console.telnet_read())` pattern
- Keep existing `handle_serial()` logic but rename to `handle_console(char c)` matching onoff pattern
- Ensure `console.loop()` is called in `loop()` before serial/telnet reads

- [ ] **Step 3: Add /docs route in main.cpp**

Add route in `setup()`:
```cpp
s_server.on("/docs", []()
    { s_server.send(200, "text/html", FPSTR(PAGE_DOCS)); });
```

- [ ] **Step 4: Add /api/settings endpoint in main.cpp**

Add a handler `handle_api_settings()` that:
- `GET /api/settings` returns `{"device_name": "...", "available_pins": []}`
- `POST /api/settings` accepts `{"device_name": "..."}`, saves to EEPROM with `save_device_name()`
- Register route: `s_server.on("/api/settings", HTTP_ANY, handle_api_settings);`

The `save_device_name()` function already exists in main.cpp — just expose it via the API.

- [ ] **Step 5: Add state counters to main.cpp**

Add near other state variables:
```cpp
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;
```

Increment `s_espnow_tx_count++` in `espnow_send_cb()` and `s_espnow_rx_count++` in `espnow_recv_cb()` (at the top, before the null check).

Add to `handle_api_state()`:
```cpp
doc["tx_count"] = s_espnow_tx_count;
doc["rx_count"] = s_espnow_rx_count;
doc["free_heap"] = ESP.getFreeHeap();
```

- [ ] **Step 6: Build and verify**

```bash
cd /mnt/c/project/homeware && pio run -d clients/esp8266_chuva 2>&1 | tail -10
```

Expected: SUCCESS (RAM < 50%, Flash < 60%)

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_chuva/ && git commit -m "feat(chuva): redesign dashboard with sidebar, console module, docs"
```

---

### Task 3: Redesign dht_gas dashboard + main.cpp

**Files:**
- Rewrite: `clients/esp8266_dht_gas/include/pages.h`
- Modify: `clients/esp8266_dht_gas/src/main.cpp`
- Modify: `clients/esp8266_dht_gas/include/espnow_protocol.h`

**Interfaces:**
- Consumes: Console module from Task 1
- Produces: New dashboard HTML at `/`, docs at `/docs`, settings API at `/api/settings`

- [ ] **Step 1: Sync espnow_protocol.h**

Copy from gateway version to add missing types:
```bash
cp /mnt/c/project/homeware/gateway/include/espnow_protocol.h /mnt/c/project/homeware/clients/esp8266_dht_gas/include/espnow_protocol.h
```
Then remove the local `payload_dht_gas_t` redefinition in main.cpp (lines ~17-22).

- [ ] **Step 2: Rewrite pages.h**

Write `clients/esp8266_dht_gas/include/pages.h` with:
- Sidebar: Home · Propriedades · Configurações
- Stats-header: RX · TX · Mem · Temp (temperature)
- Hero: temperature + humidity as large side-by-side cards + gas level badge (Seguro/Atenção/Vazamento)
- Collapsible "Detalhes": Gás %, Alarme, DHT pin, Bateria, RSSI, IP, Versão FW, Slot, Gateway
- Footer: dot + gateway status · time · uptime
- `/docs` page
- WiFi config page
- Polling 3s

JS state variable mapping:
```
tempDisplay       → d.temperature + "°C"
humDisplay        → d.humidity + "%"
gasLevel div      → d.gas_level + "%"
gasBadge span     → Seguro (<15) / Atenção (15-30) / Vazamento (>30) based on d.gas_level + d.alarm
alarmLed span     → d.alarm ? "ALARME" : "OK"
dhtPin span       → "GPIO " + d.dht_pin
battery span      → d.battery + "%"
rssi span         → d.rssi + " dBm"
ip span           → d.ip
fwVersion span    → d.fw_version
slot span         → d.slot
gatewayStatus     → d.gateway_connected ? "Online" : "Offline"
rxVal, txVal, memVal → d.rx_count, d.tx_count, d.free_heap
tempVal           → d.temperature + "°C" (for stats-header Temp stat)
deviceName input  → d.device_name
```

- [ ] **Step 3: Add console to main.cpp**

Same as Task 2 Step 2 — add `#include "console.h"`, replace inline serial with `console.begin()`/`console.loop()`/`handle_console(console.telnet_read())`.

- [ ] **Step 4: Add /docs and /api/settings routes**

Same as Task 2 Steps 3 and 4.

- [ ] **Step 5: Add state counters**

Same as Task 2 Step 5.

- [ ] **Step 6: Build and verify**

```bash
cd /mnt/c/project/homeware && pio run -d clients/esp8266_dht_gas 2>&1 | tail -10
```

Expected: SUCCESS (RAM < 50%, Flash < 60%)

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_dht_gas/ && git commit -m "feat(dht_gas): redesign dashboard with sidebar, console module, docs"
```

---

### Task 4: Redesign repeater dashboard + main.cpp

**Files:**
- Rewrite: `clients/esp8266_repeater/include/pages.h`
- Modify: `clients/esp8266_repeater/src/main.cpp`

**Interfaces:**
- Consumes: Console module from Task 1
- Produces: New dashboard at `/`, docs at `/docs`, settings at `/api/settings`

- [ ] **Step 1: Rewrite pages.h**

Write `clients/esp8266_repeater/include/pages.h` with:
- Sidebar: Home · Configurações (no Propriedades — repeater não tem sensores)
- Stats-header: RX · TX · Clients (no Mem stat)
- Hero: grid 2×2 of network stats (Recebidos, Encaminhados, Clientes Ativos, Última Atividade)
- No collapsible Detalhes section — all info visible in hero grid + stats
- Client MAC list table in main content
- Footer: dot + gateway status · time · uptime
- `/docs` page
- WiFi config page
- Polling 3s

JS state variable mapping (from `/api/status`):
```
rxVal           → d.received
txVal           → d.forwarded
clientsVal      → d.clients
gatewayMac span → d.gateway
uptime span     → formatted from d.uptime_s
channel span    → d.channel
rssi span       → d.rssi + " dBm"
activity span   → d.last_activity_s + "s ago"
clientTable     → render rows from d.client_list[]
deviceName input → d.device_name
```

- [ ] **Step 2: Add console to main.cpp**

Same pattern as Task 2 — add `#include "console.h"`, replace inline serial handling.

- [ ] **Step 3: Add /docs route**

```cpp
s_server.on("/docs", []()
    { s_server.send(200, "text/html", FPSTR(PAGE_DOCS)); });
```

- [ ] **Step 4: Add /api/settings endpoint**

Same as Task 2 Step 4 — GET returns device name, POST saves it. Repeater uses EEPROM layout with `gateway_mac` field, so save to the name area (address 10 as in other clients).

- [ ] **Step 5: Add state counters**

Add `s_espnow_rx_count` and `s_espnow_tx_count` incremented in send/recv callbacks (repeater already has `s_received`/`s_forwarded` — just add the standard-named counters too for consistency).

Add to `/api/status`:
```cpp
doc["tx_count"] = s_espnow_tx_count;
doc["rx_count"] = s_espnow_rx_count;
```

- [ ] **Step 6: Build and verify**

```bash
cd /mnt/c/project/homeware && pio run -d clients/esp8266_repeater 2>&1 | tail -10
```

Expected: SUCCESS (RAM < 50%, Flash < 60%)

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_repeater/ && git commit -m "feat(repeater): redesign dashboard with sidebar, console module, docs"
```
