# Custom Captive Portal (replace WiFiManager) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `tzapu/WiFiManager` dependency in the ESP-NOW gateway with a self-contained, style-guide-styled captive portal (WiFi + MQTT onboarding) served by the existing web server.

**Architecture:** A new `captive_portal` module starts the softAP + a minimal UDP/53 DNS responder and runs a blocking setup-time serve loop on the existing `MyWebServer`. The portal page (`PAGE_PORTAL`) posts to `POST /api/portal/setup`, which persists WiFi + MQTT to the existing EEPROM stores. `web_server_init()` is moved before `web_server_wifi_setup()` so the server is live during onboarding.

**Tech Stack:** PlatformIO (ESP32 + ESP8266), Arduino framework, ArduinoJson, the existing `MyWebServer` (ESP8266WebServer/ESP32WebServer), EEPROM WiFi/MQTT stores.

## Global Constraints

- Remove `tzapu/WiFiManager@^2.0.17` from `lib_deps` in **all four** environments of `platformio.ini`.
- Portal UI must use the project style guide CSS variables: `--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--border-strong:#d1d5db;--danger:#dc2626;--success:#16a34a`.
- Portal collects **WiFi (SSID/pass + DHCP or static IP) AND MQTT (host/port/user/pass)**.
- AP SSID/Pass from `config.h`: `WIFI_CONFIG_PORTAL_SSID` / `WIFI_CONFIG_PORTAL_PASS`; AP default IP `192.168.4.1`.
- Portal timeout = **300000 ms**; on timeout or successful submit the device calls `ESP.restart()`.
- Reuse exactly: `wifi_creds_save(ssid, pass)`, `wifi_net_save(mode, ip, gw, mask, dns)`, `mqtt_client_save_config(host, port, user, pass)`, `apply_wifi_static_ip()`, `WIFI_MODE_DHCP`/`WIFI_MODE_STATIC`.
- Branch: `dev`. Keep commits small and frequent.
- **No automated unit-test framework exists in this repo.** The "test" for every task is: (a) `pio run` builds clean for `esp32_gateway` and `esp8266_gateway`, and (b) for runtime tasks, flash + a serial/HTTP verification command with the expected output shown. This is stated explicitly so implementers do not invent a test harness.

---

## File Structure

- **Create** `include/captive_portal.h` — public API (`captive_portal_start`, `captive_portal_run`, `captive_dns_poll`, `captive_portal_submitted`, `captive_portal_set_submitted`).
- **Create** `src/captive_portal.cpp` — softAP start, captive DNS responder, blocking run loop, submit flag.
- **Modify** `src/web_server.cpp` — remove `#include <WiFiManager.h>` and `wifi_creds_capture_from_driver`; replace portal block in `web_server_wifi_setup`; gate `/` route; add portal routes + `onNotFound`; poll DNS in `web_server_loop`; add `web_server_handle_client()`.
- **Modify** `include/web_server.h` — declare `web_server_handle_client()`.
- **Modify** `src/main.cpp` — call `web_server_init()` **before** `web_server_wifi_setup(false)`.
- **Modify** `platformio.ini` — drop `tzapu/WiFiManager@^2.0.17` from all 4 envs.
- **Modify** `include/pages.h` — add styled `PAGE_PORTAL` (replaces the upstream WiFiManager HTML).
- **Modify** `include/pages.h` (`PAGE_DOCS`) — document `/api/portal/setup` and `/api/portal/scan` (Task 5).

---

### Task 1: Remove WiFiManager and scaffold the captive_portal module (builds, minimal AP)

**Files:**
- Modify: `platformio.ini` (4 `lib_deps` blocks)
- Modify: `src/web_server.cpp` (include, `wifi_creds_capture_from_driver`, `web_server_wifi_setup` portal block, `/` route, `web_server_loop`, add `web_server_handle_client` + portal routes)
- Modify: `include/web_server.h`
- Modify: `src/main.cpp`
- Create: `include/captive_portal.h`
- Create: `src/captive_portal.cpp`
- Modify: `include/pages.h` (add minimal `PAGE_PORTAL`)

**Interfaces:**
- Consumes: `wifi_creds_save`, `wifi_net_save`, `mqtt_client_save_config`, `config.h` (`WIFI_CONFIG_PORTAL_SSID/PASS`, `WIFI_MODE_DHCP/STATIC`), `serve_pgm_page`, `s_server`, `s_wifi_config_mode`, `s_wifi_config_start`.
- Produces: `captive_portal_start()`, `captive_portal_run()`, `captive_dns_poll()` (stub), `captive_portal_submitted()`, `captive_portal_set_submitted()`; `web_server_handle_client()`.

- [ ] **Step 1: Drop WiFiManager from `platformio.ini`**

In each of the four `lib_deps` blocks, delete the line `    tzapu/WiFiManager@^2.0.17`. After the edit the `esp32_gateway` block looks like:
```ini
[env:esp32_gateway]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    bogdankoci/ArduinoSTL@^1.0.1
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^7.1.0
    ...
```
Repeat identically for `esp32_gateway_ota`, `esp8266_gateway`, `esp8266_gateway_ota`.

- [ ] **Step 2: Create `include/captive_portal.h`**

```cpp
#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>

void captive_portal_start();
void captive_portal_run();
void captive_dns_poll();
bool captive_portal_submitted();
void captive_portal_set_submitted();

#endif
```

- [ ] **Step 3: Create `src/captive_portal.cpp` (stub DNS, working run loop)**

```cpp
#include "captive_portal.h"
#include "config.h"
#include "console.h"
#include <WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP s_dns;
static bool s_submitted = false;
static unsigned long s_start = 0;

void captive_portal_start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS);
    s_dns.begin(53);
    s_start = millis();
    console.printf("[PORTAL] AP '%s' IP %s\n",
        WIFI_CONFIG_PORTAL_SSID, WiFi.softAPIP().toString().c_str());
}

void captive_dns_poll() {
    // Implemented in Task 3
}

bool captive_portal_submitted() {
    return s_submitted;
}

void captive_portal_set_submitted() {
    s_submitted = true;
}

void captive_portal_run() {
    while (true) {
        web_server_handle_client();
        captive_dns_poll();
        if (s_submitted) {
            console.println("[PORTAL] Config salva, reiniciando...");
            delay(300);
            ESP.restart();
        }
        if (millis() - s_start > 300000) {
            console.println("[PORTAL] Timeout, reiniciando...");
            delay(300);
            ESP.restart();
        }
        yield();
    }
}
```

- [ ] **Step 4: Add `web_server_handle_client()` to `web_server.h` and `web_server.cpp`**

In `include/web_server.h`, after `void web_server_loop();` add:
```cpp
void web_server_handle_client();
```

In `src/web_server.cpp`, implement (e.g. after `web_server_loop`):
```cpp
void web_server_handle_client() {
    s_server.handleClient();
}
```

- [ ] **Step 5: Remove WiFiManager from `web_server.cpp`**

Delete the line `#include <WiFiManager.h>` (near the top includes).

Delete the function `wifi_creds_capture_from_driver()` (it only existed for the WiFiManager portal and is now unused):
```cpp
// DELETE THIS ENTIRE FUNCTION
static void wifi_creds_capture_from_driver() {
    wifi_config_t cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
        if (strlen((char*)cfg.sta.ssid) > 0) {
            wifi_creds_save((char*)cfg.sta.ssid, (char*)cfg.sta.password);
        }
    }
}
```

- [ ] **Step 6: Replace the portal block in `web_server_wifi_setup`**

Replace the block starting at `console.println("[WIFI] Starting config portal...");` through the end of the function (the `wifiManager.startConfigPortal(...)` section) with:
```cpp
    console.println("[WIFI] Starting config portal...");
    s_wifi_config_mode = true;
    s_wifi_config_start = millis();
    captive_portal_start();
    captive_portal_run();
    // captive_portal_run() only returns after ESP.restart(); this is a fallback:
    s_wifi_config_mode = false;
    return false;
```
Add `#include "captive_portal.h"` near the other includes in `web_server.cpp`.

- [ ] **Step 7: Gate the `/` route and add portal routes in `web_server_init()`**

Change the existing `/` handler:
```cpp
    s_server.on("/", HTTP_GET, []() {
        if (s_wifi_config_mode) serve_pgm_page(PAGE_PORTAL);
        else serve_pgm_page(PAGE_SHELL);
    });
```

Add these routes (place them just before `s_server.begin();` at the end of `web_server_init`):
```cpp
    s_server.on("/api/portal/setup", HTTP_POST, []() {
        s_server.send(501, "application/json", "{\"error\":\"not implemented\"}");
    });
    s_server.on("/api/portal/scan", HTTP_GET, []() {
        s_server.send(501, "application/json", "{\"error\":\"not implemented\"}");
    });
    auto portal_redirect = []() {
        s_server.sendHeader("Location", "/");
        s_server.send(302, "text/plain", "");
    };
    s_server.on("/generate_204", HTTP_GET, portal_redirect);
    s_server.on("/hotspot-detect.html", HTTP_GET, portal_redirect);
    s_server.on("/ncsi.txt", HTTP_GET, portal_redirect);
    s_server.on("/success.html", HTTP_GET, portal_redirect);
    s_server.onNotFound([]() {
        if (s_wifi_config_mode) {
            s_server.sendHeader("Location", "/");
            s_server.send(302, "text/plain", "");
        } else {
            s_server.send(404, "text/plain", "Not found");
        }
    });
```

- [ ] **Step 8: Poll captive DNS in `web_server_loop()`**

In `web_server_loop()`, after `s_server.handleClient();` add:
```cpp
    if (s_wifi_config_mode) captive_dns_poll();
```
(At runtime the portal blocks in `captive_portal_run()`, but this keeps DNS alive if the loop is ever the serving path.)

- [ ] **Step 9: Move `web_server_init()` before `web_server_wifi_setup()` in `main.cpp`**

In `setup()`, move the `web_server_init();` call (currently after the `web_server_wifi_setup` check) to **before** the `web_server_wifi_setup(false)` call. Resulting order:
```cpp
    sensor_registry_init();
    mqtt_client_load_config();
    mqtt_client_generate_device_ids();

    web_server_init();

    if (!web_server_wifi_setup(false)) {
        console.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }

    ota_init(get_gateway_device_id());
    console.begin();
    espnow_handler_init();
    log_buffer_init();

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ...
```
(Do NOT change the force-portal path `web_server_wifi_setup(true)` or anything else.)

- [ ] **Step 10: Add a minimal `PAGE_PORTAL` to `include/pages.h`**

Add (anywhere among the page constants, e.g. right before `PAGE_SHELL`):
```cpp
const char PAGE_PORTAL[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-BR"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configurar Gateway</title></head>
<body style="font-family:sans-serif;padding:24px">
<h1>Configurar Wi-Fi (modo portal)</h1>
<p>Endpoint /api/portal/setup implementado na Task 2.</p>
</body></html>
)rawliteral";
```

- [ ] **Step 11: Build both environments**

Run:
```bash
pio run -e esp32_gateway && pio run -e esp8266_gateway
```
Expected: both succeed (WiFiManager symbols gone). If a `wifiManager`/`WiFiManager` reference remains, the build fails with "not declared" — fix by removing it.

- [ ] **Step 12: Commit**

```bash
git add platformio.ini src/web_server.cpp include/web_server.h src/main.cpp include/captive_portal.h src/captive_portal.cpp include/pages.h
git commit -m "feat: remove WiFiManager, scaffold captive_portal module (no UI yet)"
```

---

### Task 2: Styled `PAGE_PORTAL` + `POST /api/portal/setup` + captive detection

**Files:**
- Modify: `include/pages.h` (`PAGE_PORTAL` full styled page with JS)
- Modify: `src/web_server.cpp` (`/api/portal/setup` handler, real detection routes already wired in Task 1)

**Interfaces:**
- Consumes: `wifi_creds_save`, `wifi_net_save`, `mqtt_client_save_config`, `WIFI_MODE_DHCP/STATIC`, `MQTT_PORT_DEFAULT`, `captive_portal_set_submitted()`, `captive_portal_submitted()`.
- Produces: HTTP `POST /api/portal/setup` accepting JSON `{ssid,pass,mode,ip,gateway,subnet,dns,mqtt_host,mqtt_port,mqtt_user,mqtt_pass}`; styled portal page.

- [ ] **Step 1: Implement `POST /api/portal/setup` in `web_server.cpp`**

Replace the Task-1 stub handler with:
```cpp
    s_server.on("/api/portal/setup", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("ssid")) {
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        const char *ssid = doc["ssid"];
        const char *pass = doc["pass"] | "";
        int mode = doc["mode"] | WIFI_MODE_DHCP;
        const char *ip = doc["ip"] | "";
        const char *gw = doc["gateway"] | "";
        const char *mask = doc["subnet"] | "";
        const char *dns = doc["dns"] | "";
        if (strlen(ssid) == 0) {
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        if (mode == WIFI_MODE_STATIC && (strlen(ip) == 0 || strlen(gw) == 0)) {
            s_server.send(400, "application/json", "{\"error\":\"static ip and gateway required\"}");
            return;
        }
        const char *mqtt_host = doc["mqtt_host"] | "";
        uint16_t mqtt_port = doc["mqtt_port"] | MQTT_PORT_DEFAULT;
        const char *mqtt_user = doc["mqtt_user"] | "";
        const char *mqtt_pass = doc["mqtt_pass"] | "";
        if (strlen(mqtt_host) > 0) {
            mqtt_client_save_config(mqtt_host, mqtt_port, mqtt_user, mqtt_pass);
        }
        wifi_creds_save(ssid, pass);
        wifi_net_save(mode, ip, gw, mask, dns);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        captive_portal_set_submitted();
    });
```

- [ ] **Step 2: Replace `PAGE_PORTAL` with the styled page**

Replace the entire `PAGE_PORTAL` constant with:
```cpp
const char PAGE_PORTAL[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Configurar Gateway</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--border-strong:#d1d5db;--danger:#dc2626;--success:#16a34a}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:420px}
h1{font-size:1.1rem;font-weight:700;color:var(--primary);margin-bottom:4px}
.sub{color:var(--muted);font-size:0.8rem;margin-bottom:20px}
.form-group{margin-bottom:14px}
.form-group label{display:block;margin-bottom:4px;font-size:0.82rem;color:var(--muted)}
input,select{width:100%;padding:10px 12px;border:1px solid var(--border);border-radius:8px;font-size:16px;background:var(--surface-2);color:var(--text)}
.toggle-row{display:flex;gap:8px;margin:10px 0}
.seg{flex:1;padding:10px;border:1px solid var(--border);background:var(--surface-2);color:var(--muted);border-radius:8px;font-weight:600;cursor:pointer;font-size:0.85rem}
.seg.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.hidden{display:none!important}
.btn{padding:12px;width:100%;border:none;border-radius:8px;font-weight:600;cursor:pointer;font-size:0.9rem;background:var(--primary);color:#fff}
.section{margin-top:18px;padding-top:14px;border-top:1px solid var(--border)}
.section h2{font-size:0.9rem;color:var(--primary);margin-bottom:12px}
</style>
</head>
<body>
<div class="card">
<h1>Configurar Gateway</h1>
<div class="sub">Conecte o gateway à sua rede Wi-Fi e ao broker MQTT</div>
<div class="form-group">
<label>SSID</label>
<input list="ssid-list" id="ssid" placeholder="Nome da rede" maxlength="32">
<datalist id="ssid-list"></datalist>
</div>
<div class="form-group">
<label>Senha</label>
<input type="password" id="pass" placeholder="Senha do Wi-Fi" maxlength="64">
</div>
<div class="toggle-row">
<button type="button" class="seg active" id="seg-dhcp" onclick="setMode(0)">DHCP</button>
<button type="button" class="seg" id="seg-static" onclick="setMode(1)">IP Fixo</button>
</div>
<div id="static-fields" class="hidden">
<div class="form-group"><label>IP Fixo</label><input id="ip" placeholder="192.168.1.50"></div>
<div class="form-group"><label>Gateway</label><input id="gw" placeholder="192.168.1.1"></div>
<div class="form-group"><label>Máscara</label><input id="mask" placeholder="255.255.255.0"></div>
<div class="form-group"><label>DNS</label><input id="dns" placeholder="192.168.1.1"></div>
</div>
<div class="section">
<h2>MQTT</h2>
<div class="form-group"><label>Broker IP</label><input id="mqtt_host" placeholder="192.168.1.12"></div>
<div class="form-group"><label>Porta</label><input id="mqtt_port" placeholder="1883"></div>
<div class="form-group"><label>Usuário</label><input id="mqtt_user" placeholder="(opcional)"></div>
<div class="form-group"><label>Senha</label><input type="password" id="mqtt_pass" placeholder="(opcional)"></div>
</div>
<button class="btn" id="save" onclick="save()">Salvar e Reiniciar</button>
</div>
<script>
var mode = 0;
function setMode(m){mode=m;document.getElementById('seg-dhcp').classList.toggle('active',m===0);document.getElementById('seg-static').classList.toggle('active',m===1);document.getElementById('static-fields').classList.toggle('hidden',m!==1);}
function save(){
  var ssid=document.getElementById('ssid').value.trim();
  if(!ssid){alert('SSID obrigatório');return;}
  if(mode===1 && (!document.getElementById('ip').value.trim() || !document.getElementById('gw').value.trim())){alert('IP Fixo e Gateway obrigatórios');return;}
  var body={ssid:ssid,pass:document.getElementById('pass').value,mode:mode,
    ip:document.getElementById('ip').value.trim(),gateway:document.getElementById('gw').value.trim(),
    subnet:document.getElementById('mask').value.trim(),dns:document.getElementById('dns').value.trim(),
    mqtt_host:document.getElementById('mqtt_host').value.trim(),
    mqtt_port:parseInt(document.getElementById('mqtt_port').value)||1883,
    mqtt_user:document.getElementById('mqtt_user').value.trim(),
    mqtt_pass:document.getElementById('mqtt_pass').value};
  fetch('/api/portal/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){if(!r.ok)throw new Error('erro');return r.json();})
    .then(function(){document.getElementById('save').textContent='Salvo! Reiniciando...';})
    .catch(function(){alert('Falha ao salvar');});
}
</script>
</body>
</html>
)rawliteral";
```

- [ ] **Step 3: Build**

```bash
pio run -e esp32_gateway
```
Expected: success.

- [ ] **Step 4: Flash + verify portal submit (DHCP) connects**

Flash to the device (after a full erase so it enters the portal):
```bash
pio run -e esp32_gateway -t erase
pio run -e esp32_gateway -t upload
```
Connect a laptop/phone to the `ESPNOW_Gateway_Setup` AP, open `http://192.168.4.1`, fill SSID + password + MQTT (`192.168.1.12:1883`, user `kzuca`), click Salvar. Expected: device reboots and joins the LAN; `scan.py` (run from repo root `python3 scan.py`) finds the gateway with an IP and MQTT connected.

- [ ] **Step 5: Commit**

```bash
git add include/pages.h src/web_server.cpp
git commit -m "feat: styled portal page + POST /api/portal/setup (WiFi+MQTT)"
```

---

### Task 3: Captive DNS responder (UDP/53)

**Files:**
- Modify: `src/captive_portal.cpp` (`captive_dns_poll` implementation)

**Interfaces:**
- Consumes: `WiFi.softAPIP()`, `WiFiUDP`.
- Produces: DNS replies pointing all queries to the AP IP so devices show "sign-in required".

- [ ] **Step 1: Implement `captive_dns_poll()`**

Replace the stub `captive_dns_poll()` body in `src/captive_portal.cpp` with:
```cpp
void captive_dns_poll() {
    if (s_dns.parsePacket() == 0) return;
    static uint8_t buf[512];
    int len = s_dns.read(buf, sizeof(buf));
    if (len < 12) return;
    IPAddress ap = WiFi.softAPIP();
    // Header: copy ID; set QR=1, AA=1, RA=1, RCODE=0
    buf[2] = 0x81;
    buf[3] = 0x80;
    buf[6] = 0x00; buf[7] = 0x01; // ANCOUNT = 1
    buf[8] = 0x00; buf[9] = 0x00;  // NSCOUNT = 0
    buf[10] = 0x00; buf[11] = 0x00; // ARCOUNT = 0
    // Find end of question section
    int pos = 12;
    while (pos < len && buf[pos] != 0) pos += buf[pos] + 1;
    pos += 1 + 4; // null terminator + QTYPE(2) + QCLASS(2)
    if (pos + 16 > sizeof(buf)) return;
    // Answer: name pointer 0xC00C, TYPE A, CLASS IN, TTL 60, RDLEN 4, RDATA = AP IP
    buf[pos++] = 0xC0; buf[pos++] = 0x0C;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x3C;
    buf[pos++] = 0x00; buf[pos++] = 0x04;
    buf[pos++] = ap[0]; buf[pos++] = ap[1]; buf[pos++] = ap[2]; buf[pos++] = ap[3];
    s_dns.beginPacket(s_dns.remoteIP(), 53);
    s_dns.write(buf, pos);
    s_dns.endPacket();
}
```

- [ ] **Step 2: Build**

```bash
pio run -e esp32_gateway
```
Expected: success.

- [ ] **Step 3: Verify captive detection**

Flash, connect phone to the AP, open any http URL (e.g. `http://example.com`). Expected: the phone's browser is redirected to the portal and shows "sign-in required" / captive portal notification. (Manual check; no automated assertion.)

- [ ] **Step 4: Commit**

```bash
git add src/captive_portal.cpp
git commit -m "feat: captive DNS responder (UDP/53 -> AP IP)"
```

---

### Task 4: WiFi scan + SSID dropdown

**Files:**
- Modify: `src/web_server.cpp` (`/api/portal/scan` handler)
- Modify: `include/pages.h` (`PAGE_PORTAL` JS: populate datalist on load)

**Interfaces:**
- Consumes: `WiFi.scanNetworks()`, `WiFi.SSID(i)`, `WiFi.RSSI(i)`, `WiFi.encryptionType(i)`.
- Produces: `GET /api/portal/scan` returning `{networks:[{ssid,rssi,enc}, ...]}`.

- [ ] **Step 1: Implement `GET /api/portal/scan`**

Replace the Task-1 stub handler with:
```cpp
    s_server.on("/api/portal/scan", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = WiFi.SSID(i);
            o["rssi"] = WiFi.RSSI(i);
            o["enc"] = (int)WiFi.encryptionType(i);
        }
        WiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });
```

- [ ] **Step 2: Populate the SSID datalist in `PAGE_PORTAL`**

In `PAGE_PORTAL`, add this `<script>` content at the start of the existing `<script>` block (before `var mode = 0;`):
```javascript
fetch('/api/portal/scan').then(function(r){return r.json();}).then(function(d){
  var dl=document.getElementById('ssid-list');
  (d.networks||[]).forEach(function(n){
    if(!n.ssid)return;
    var o=document.createElement('option');o.value=n.ssid;dl.appendChild(o);
  });
}).catch(function(){});
```

- [ ] **Step 3: Build**

```bash
pio run -e esp32_gateway
```
Expected: success.

- [ ] **Step 4: Verify scan populates dropdown**

Flash, open portal, click the SSID field. Expected: a dropdown of nearby SSIDs appears (from `/api/portal/scan`).

- [ ] **Step 5: Commit**

```bash
git add src/web_server.cpp include/pages.h
git commit -m "feat: WiFi scan endpoint + SSID dropdown in portal"
```

---

### Task 5: Full verification + docs

**Files:**
- Modify: `include/pages.h` (`PAGE_DOCS` add `/api/portal/setup` and `/api/portal/scan`)
- Verify: erase+flash full flow, static-IP path, force-portal path.

**Interfaces:**
- Consumes: existing docs page structure.

- [ ] **Step 1: Document the new endpoints in `PAGE_DOCS`**

In `PAGE_DOCS`, add a "Portal de Configuração" section after the "Rede (WiFi)" section:
```html
<h2>Portal de Configuração</h2>
<div class="endpoint">
<div class="head"><span class="method post">POST</span><span class="path">/api/portal/setup</span></div>
<div class="desc">Configura WiFi (SSID/senha/IP fixo ou DHCP) + MQTT no modo portal cativo. Reinicia o gateway</div>
</div>
<div class="endpoint">
<div class="head"><span class="method get">GET</span><span class="path">/api/portal/scan</span></div>
<div class="desc">Lista redes Wi-Fi visíveis (usado pelo portal)</div>
</div>
```

- [ ] **Step 2: Full erase + flash + portal (DHCP) flow**

```bash
pio run -e esp32_gateway -t erase
pio run -e esp32_gateway -t upload
```
Connect to AP, submit DHCP WiFi + MQTT. Expected: device joins LAN, `scan.py` shows it online with MQTT connected.

- [ ] **Step 3: Static-IP flow**

Re-enter portal (full erase again, or use dashboard "Configurar Rede" after connecting), choose IP Fixo with a valid IP+Gateway. Expected: after reboot the gateway uses the fixed IP (visible in `scan.py` and dashboard `/settings`).

- [ ] **Step 4: Force-portal path**

From a serial console on the running device, trigger the force-portal path (long-press the pair button ~3s → `web_server_wifi_setup(true)`). Expected: device enters the custom portal (AP `ESPNOW_Gateway_Setup`) and the styled page loads.

- [ ] **Step 5: Build ESP8266 env**

```bash
pio run -e esp8266_gateway
```
Expected: success (WiFiManager fully removed, no undefined references on ESP8266 either).

- [ ] **Step 6: Commit**

```bash
git add include/pages.h
git commit -m "docs: document portal endpoints; final captive-portal verification"
```

---

## Self-Review Notes

- **Spec coverage:** WiFiManager removal (Task 1) ✓; `captive_portal` module + `PAGE_PORTAL` (Tasks 1–2) ✓; `web_server_init` reorder (Task 1) ✓; WiFi+MQTT capture (Task 2) ✓; captive DNS (Task 3) ✓; detection routes + `onNotFound` (Task 1) ✓; WiFi scan (Task 4) ✓; static-IP support via `wifi_net_save` + `apply_wifi_static_ip` (Task 2, already wired in `web_server_wifi_setup`) ✓; verification incl. static-IP and force-portal (Task 5) ✓.
- **No placeholders:** every code step shows full code; verification steps give exact commands + expected outcomes.
- **Type consistency:** `captive_portal_set_submitted()` declared in header and used in Task 2 handler; `web_server_handle_client()` declared in `web_server.h` and called in `captive_portal_run()`; `s_wifi_config_mode`/`s_wifi_config_start` reused from `web_server.cpp`; `WIFI_MODE_DHCP`/`WIFI_MODE_STATIC` from `config.h`. All names match across tasks.
