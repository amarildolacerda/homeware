# ESP8266 Clients Dashboard Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or supabase:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign chuva, dht_gas, repeater dashboards to match the sidebar layout of lampada/onoff, add missing /docs, /api/settings, console module, and fix the hardcoded version bug in lampada/onoff.

**Architecture:** Each client is independent — no shared code. Console module copied from onoff with client-specific welcome message. pages.h rewritten per client with sidebar, stats-header, footer bar, and client-specific hero/state rendering. lampada/onoff get minimal fixes (version bug).

**Tech Stack:** ESP8266 Arduino, ESP8266WebServer, ArduinoJson, WiFiManager

## Global Constraints

- Dashboard uses sidebar layout (180px desktop, 48px mobile collapse) matching onoff/lampada
- Stats-header shows RX · TX · Mem + 1 client-specific stat
- Footer shows dot + gateway status · time · uptime
- `/docs` page lists all API endpoints in card layout
- `pages.h` must stay under 8KB for `send_P()`
- Console module matches onoff API: `begin()`, `loop()`, `printf()`, `telnet_available()`, `telnet_read()`
- No changes to sensor logic, ESP-NOW protocol, pairing, or WiFiManager
- FW_VERSION = v0.0.22 (current)
- CSS variables from style-guide: `--bg:#f4f4f4`, `--surface:#fff`, `--primary:#5e6ad2`, etc.

## File Map

| Client | pages.h | console.h | console.cpp | main.cpp | SPEC.md |
|--------|---------|-----------|-------------|----------|---------|
| chuva | Rewrite | Create | Create | Modify | Create |
| dht_gas | Rewrite | Create | Create | Modify | Create |
| repeater | Rewrite | Create | Create | Modify | — (exists) |
| lampada | Fix version | — | — | — | — |
| onoff | Fix version | — | — | — | — |

---

### Task 1: Fix hardcoded version in lampada and onoff

**Files:**
- Modify: `clients/esp8266_lampada/include/pages.h:84`
- Modify: `clients/esp8266_onoff/include/pages.h:84`

- [ ] **Step 1: Fix lampada sidebar version**

In `clients/esp8266_lampada/include/pages.h`, line 84, replace:
```html
<span id="sbVersion">v0.0.21</span>
```
with:
```html
<span id="sbVersion">...</span>
```
The JS already populates `sbVersion` from `d.fw_version` in the state fetch — verify this exists. If not, add to the state fetch handler:
```js
const ve=document.getElementById('sbVersion');if(ve&&d.fw_version)ve.textContent=d.fw_version;
```

- [ ] **Step 2: Fix onoff sidebar version**

Same change in `clients/esp8266_onoff/include/pages.h:84`.

- [ ] **Step 3: Build both**

```bash
pio run -d clients/esp8266_lampada 2>&1 | tail -5
pio run -d clients/esp8266_onoff 2>&1 | tail -5
```

Expected: both SUCCESS

- [ ] **Step 4: Commit**

```bash
git add clients/esp8266_lampada/include/pages.h clients/esp8266_onoff/include/pages.h
git commit -m "fix(lampada,onoff): replace hardcoded v0.0.21 with dynamic fw_version"
```

---

### Task 2: Create console module for chuva, dht_gas, repeater

**Files:**
- Create: `clients/esp8266_chuva/include/console.h` (copy from onoff)
- Create: `clients/esp8266_chuva/src/console.cpp` (copy from onoff)
- Create: `clients/esp8266_dht_gas/include/console.h` (copy from onoff)
- Create: `clients/esp8266_dht_gas/src/console.cpp` (copy from onoff)
- Create: `clients/esp8266_repeater/include/console.h` (copy from onoff)
- Create: `clients/esp8266_repeater/src/console.cpp` (copy from onoff)

- [ ] **Step 1: Copy console files to chuva**

```bash
cp clients/esp8266_onoff/include/console.h clients/esp8266_chuva/include/console.h
cp clients/esp8266_onoff/src/console.cpp clients/esp8266_chuva/src/console.cpp
```

Edit `clients/esp8266_chuva/src/console.cpp:16` — change welcome message:
```cpp
m_client.printf("\r\n=== ESP8266 Chuva %s ===\r\n", FW_VERSION);
```

- [ ] **Step 2: Copy console files to dht_gas**

```bash
cp clients/esp8266_onoff/include/console.h clients/esp8266_dht_gas/include/console.h
cp clients/esp8266_onoff/src/console.cpp clients/esp8266_dht_gas/src/console.cpp
```

Edit `clients/esp8266_dht_gas/src/console.cpp:16`:
```cpp
m_client.printf("\r\n=== ESP8266 DHT+Gas %s ===\r\n", FW_VERSION);
```

- [ ] **Step 3: Copy console files to repeater**

```bash
cp clients/esp8266_onoff/include/console.h clients/esp8266_repeater/include/console.h
cp clients/esp8266_onoff/src/console.cpp clients/esp8266_repeater/src/console.cpp
```

Edit `clients/esp8266_repeater/src/console.cpp:16`:
```cpp
m_client.printf("\r\n=== ESP8266 Repeater %s ===\r\n", FW_VERSION);
```

- [ ] **Step 4: Commit**

```bash
git add clients/esp8266_chuva/include/console.h clients/esp8266_chuva/src/console.cpp clients/esp8266_dht_gas/include/console.h clients/esp8266_dht_gas/src/console.cpp clients/esp8266_repeater/include/console.h clients/esp8266_repeater/src/console.cpp
git commit -m "feat(chuva,dht_gas,repeater): add console module from onoff"
```

---

### Task 3: Integrate console in main.cpp (all 3 clients)

**Files:**
- Modify: `clients/esp8266_chuva/src/main.cpp`
- Modify: `clients/esp8266_dht_gas/src/main.cpp`
- Modify: `clients/esp8266_repeater/src/main.cpp`

For each client, apply the same pattern:

- [ ] **Step 1: Add console include**

After existing `#include` lines, add:
```cpp
#include "console.h"
```

- [ ] **Step 2: Add console.begin() in setup()**

After `Serial.begin(115200)` (or similar), add:
```cpp
console.begin();
```

- [ ] **Step 3: Add console.loop() in loop()**

At the start of `loop()`, add:
```cpp
console.loop();
```

- [ ] **Step 4: Replace Serial.print with console.printf**

Find all `Serial.printf(` and `Serial.println(` calls and replace with `console.printf(` and `console.println(`. This routes output to both serial and telnet.

- [ ] **Step 5: Add telnet command handling**

If the client has a `handle_serial()` function, add at the end of `loop()`:
```cpp
int c = console.telnet_read();
if (c >= 0) handle_serial((char)c);
```

- [ ] **Step 6: Build all 3**

```bash
pio run -d clients/esp8266_chuva 2>&1 | tail -5
pio run -d clients/esp8266_dht_gas 2>&1 | tail -5
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

Expected: all SUCCESS

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_chuva/src/main.cpp clients/esp8266_dht_gas/src/main.cpp clients/esp8266_repeater/src/main.cpp
git commit -m "feat(chuva,dht_gas,repeater): integrate console module in main.cpp"
```

---

### Task 4: Add /docs, /api/settings, state counters to chuva

**Files:**
- Modify: `clients/esp8266_chuva/src/main.cpp`

- [ ] **Step 1: Add /docs route in setup()**

```cpp
s_server.on("/docs", []() {
    s_server.send(200, "text/html", FPSTR(PAGE_DOCS));
});
```

- [ ] **Step 2: Add /api/settings endpoint**

Add handler function:
```cpp
void handle_api_settings() {
    if (s_server.method() == HTTP_GET) {
        DynamicJsonDocument doc(256);
        doc["device_name"] = get_device_name();
        doc["available_pins"] = JsonArray();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    } else if (s_server.method() == HTTP_POST) {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, s_server.arg("plain"));
        if (doc.containsKey("device_name")) {
            String name = doc["device_name"].as<String>();
            if (name.length() > 0) {
                save_device_name(name.c_str());
                s_server.send(200, "application/json", "{\"ok\":true}");
                return;
            }
        }
        s_server.send(400, "application/json", "{\"error\":\"invalid\"}");
    }
}
```

Register in setup():
```cpp
s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
```

- [ ] **Step 3: Add state counters**

Add near other static variables:
```cpp
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;
```

Increment in `espnow_send_cb()` and `espnow_recv_cb()`.

Add to `handle_api_state()`:
```cpp
doc["tx_count"] = s_espnow_tx_count;
doc["rx_count"] = s_espnow_rx_count;
doc["free_heap"] = ESP.getFreeHeap();
```

- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_chuva 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_chuva/src/main.cpp
git commit -m "feat(chuva): add /docs, /api/settings, state counters"
```

---

### Task 5: Add /docs, /api/settings, state counters to dht_gas

**Files:**
- Modify: `clients/esp8266_dht_gas/src/main.cpp`

Same steps as Task 4, adapted for dht_gas:

- [ ] **Step 1: Add /docs route**
- [ ] **Step 2: Add /api/settings endpoint** (same handler pattern)
- [ ] **Step 3: Add state counters** (same pattern)
- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_dht_gas 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_dht_gas/src/main.cpp
git commit -m "feat(dht_gas): add /docs, /api/settings, state counters"
```

---

### Task 6: Add /docs, /api/settings, state counters to repeater

**Files:**
- Modify: `clients/esp8266_repeater/src/main.cpp`

Note: repeater uses `/api/status` instead of `/api/state`. Adapt accordingly.

- [ ] **Step 1: Add /docs route**
- [ ] **Step 2: Add /api/settings endpoint** (same handler pattern)
- [ ] **Step 3: Add state counters** to `/api/status`
- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_repeater/src/main.cpp
git commit -m "feat(repeater): add /docs, /api/settings, state counters"
```

---

### Task 7: Redesign chuva pages.h (sidebar + docs + wifi config)

**Files:**
- Rewrite: `clients/esp8266_chuva/include/pages.h`

Reference: copy sidebar/CSS structure from `clients/esp8266_onoff/include/pages.h`, adapt content sections.

- [ ] **Step 1: Write PAGE_DASHBOARD**

Structure:
- `R"rawliteral(` ... `)rawliteral"`
- CSS: copy `:root` variables + sidebar + stats-header + footer-bar + card styles from onoff
- Sidebar nav: Home · Propriedades · Configurações
- Stats-header: RX · TX · Mem · Nível
- Hero: large centered rain level % with color badge
- Section `#secHome`: hero + collapsible "Detalhes" (digital, bateria, RSSI, IP, versão, slot, gateway)
- Section `#secPropriedades`: device name input, save button
- Section `#secConfig`: (placeholder — chuva has no configurable pins)
- Footer bar: gateway dot + status · time · uptime
- JS: `fetchState()` polling 3s, maps `d.rain_level`, `d.rain_digital`, `d.battery`, etc.

- [ ] **Step 2: Write PAGE_DOCS**

Card layout listing endpoints:
- `GET /` — Dashboard
- `GET /docs` — Esta página
- `GET /api/state` — Estado do dispositivo
- `GET /api/pin?gpio=<pin>` — Ler GPIO
- `POST /api/pin` — Setar GPIO `{"gpio":N,"state":0|1}`
- `GET /api/settings` — Configurações
- `POST /api/settings` — Atualizar `{"device_name":"..."}`
- `POST /api/ota` — Upload firmware

- [ ] **Step 3: Write PAGE_WIFI_CONFIG**

Copy from onoff's PAGE_WIFI_CONFIG (WiFiManager portal page).

- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_chuva 2>&1 | tail -5
```

Check RAM usage — must be < 50%.

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_chuva/include/pages.h
git commit -m "feat(chuva): redesign dashboard with sidebar, docs, wifi config"
```

---

### Task 8: Redesign dht_gas pages.h (sidebar + docs + wifi config)

**Files:**
- Rewrite: `clients/esp8266_dht_gas/include/pages.h`

- [ ] **Step 1: Write PAGE_DASHBOARD**

Structure:
- Sidebar nav: Home · Propriedades · Configurações
- Stats-header: RX · TX · Mem · Temp
- Hero: temperature + humidity side-by-side large values + gas level with alarm badge
- Collapsible "Detalhes": gas %, alarme, DHT pin, bateria, RSSI, IP, versão, slot, gateway
- Section `#secPropriedades`: device name input
- Section `#secConfig`: placeholder
- Footer bar: gateway dot + status · time · uptime
- JS: maps `d.temperature`, `d.humidity`, `d.gas_level`, `d.alarm`, `d.dht_valid`, etc.

- [ ] **Step 2: Write PAGE_DOCS**

Endpoints:
- `GET /`, `GET /docs`, `GET /api/state`, `GET/POST /api/pin`, `GET/POST /api/settings`, `POST /api/ota`

- [ ] **Step 3: Write PAGE_WIFI_CONFIG** (copy from onoff)

- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_dht_gas 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_dht_gas/include/pages.h
git commit -m "feat(dht_gas): redesign dashboard with sidebar, docs, wifi config"
```

---

### Task 9: Redesign repeater pages.h (sidebar + docs + wifi config)

**Files:**
- Rewrite: `clients/esp8266_repeater/include/pages.h`

- [ ] **Step 1: Write PAGE_DASHBOARD**

Structure:
- Sidebar nav: Home · Configurações (no Propriedades — repeater não tem sensores)
- Stats-header: RX · TX · Clients
- Hero: 2×2 grid — Recebidos, Encaminhados, Clientes Ativos, Última Atividade
- No collapsible detalhes — all info in hero grid
- Client MAC list table below hero
- Footer bar: gateway dot + status · time · uptime
- JS: maps `d.received`, `d.forwarded`, `d.clients`, `d.client_list[]`, `d.gateway`, `d.channel`, etc. from `/api/status`

- [ ] **Step 2: Write PAGE_DOCS**

Endpoints:
- `GET /`, `GET /docs`, `GET /api/status`, `GET/POST /api/settings`

- [ ] **Step 3: Write PAGE_WIFI_CONFIG** (copy from onoff)

- [ ] **Step 4: Build**

```bash
pio run -d clients/esp8266_repeater 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add clients/esp8266_repeater/include/pages.h
git commit -m "feat(repeater): redesign dashboard with sidebar, docs, wifi config"
```

---

### Task 10: Create missing SPEC.md files

**Files:**
- Create: `clients/esp8266_chuva/SPEC.md`
- Create: `clients/esp8266_dht_gas/SPEC.md`

- [ ] **Step 1: Write chuva SPEC.md**

Document: purpose, hardware pins, ESP-NOW protocol, API endpoints, dashboard sections, expected behavior (rain level thresholds, digital pin logic).

- [ ] **Step 2: Write dht_gas SPEC.md**

Document: purpose, hardware pins (DHT22 GPIO5, MQ-2 analog/digital), ESP-NOW protocol, API endpoints, dashboard sections, expected behavior (alarm thresholds, DHT validity fallback).

- [ ] **Step 3: Commit**

```bash
git add clients/esp8266_chuva/SPEC.md clients/esp8266_dht_gas/SPEC.md
git commit -m "docs(chuva,dht_gas): add SPEC.md"
```

---

### Task 11: Final verification — all clients build

- [ ] **Step 1: Build all 5 clients**

```bash
pio run -d clients/esp8266_chuva && pio run -d clients/esp8266_dht_gas && pio run -d clients/esp8266_repeater && pio run -d clients/esp8266_lampada && pio run -d clients/esp8266_onoff
```

Expected: all SUCCESS, RAM < 50%, Flash < 60%

- [ ] **Step 2: Verify pages.h size < 8KB each**

```bash
wc -c clients/esp8266_*/include/pages.h
```

All must be < 8192 bytes.

- [ ] **Step 3: Verify /docs exists in all main.cpp**

```bash
grep -l "/docs" clients/esp8266_*/src/main.cpp
```

Expected: 5 files.

- [ ] **Step 4: Verify no hardcoded v0.0.21 remains**

```bash
grep -r "v0.0.21" clients/
```

Expected: no results.

- [ ] **Step 5: Final commit if any fixes needed**

```bash
git add -A && git commit -m "fix: final adjustments after dashboard redesign"
```
