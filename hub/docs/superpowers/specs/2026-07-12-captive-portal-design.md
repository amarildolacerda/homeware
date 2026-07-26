# Design: Custom Captive Portal (replace WiFiManager) — ESP-NOW Gateway

- **Date:** 2026-07-12
- **Author:** kzuca
- **Status:** Approved (design)
- **Branch:** dev
- **Repo:** homeware/gateway

## Context

The gateway currently depends on `tzapu/WiFiManager@^2.0.17` (pulled from the
PlatformIO registry) used **only** for the WiFi config portal inside
`web_server_wifi_setup()` (`src/web_server.cpp`). The saved-credentials connect
path already uses direct `WiFi.begin()` + `apply_wifi_static_ip()` and does not
need WiFiManager.

The captive portal's UI is WiFiManager's upstream HTML/CSS, which does not match
the project style guide and cannot be cleanly restyled. Rather than fork
WiFiManager (`MyWiFiManager`), we will drop the dependency entirely and build a
self-contained captive portal on the gateway's existing web server, styled per
the ESP32 Bridge style guide. This removes a third-party lib and unifies all UI
under one style.

The portal collects **WiFi (SSID / password / DHCP or static IP) AND MQTT
(host / port / user / pass)** in a single onboarding screen — preserving the
behavior the WiFiManager portal had (it collected MQTT; WiFi was handled by
WiFiManager's own form).

## Goals

1. Remove `tzapu/WiFiManager` from the gateway (include + `lib_deps`).
2. Implement a captive portal served by the existing `MyWebServer s_server`.
3. Match the project style guide (same CSS variables/components as the dashboard).
4. Collect WiFi + MQTT; persist to the existing EEPROM-backed stores.
5. Trigger "sign-in required" on phones/laptops via captive DNS + detection URLs.

## Non-goals

- No separate `MyWiFiManager` git repo (decision: build portal in-repo instead).
- No changes to client ESP8266 firmwares (they keep using WiFiManager per spec).
- No change to the dashboard `/settings` WiFi/MQTT configuration (already built);
  it remains the way to change config after the device is on the LAN.

## Architecture

### New files
- `include/captive_portal.h` — public API:
  - `void captive_portal_start();` — start AP + captive DNS, set `s_wifi_config_mode`.
  - `void captive_portal_run();` — blocking serve loop until submit or timeout.
  - `void captive_dns_poll();` — process one pending DNS packet (called from `web_server_loop`).
  - `bool captive_portal_submitted();` — true after a successful `/api/portal/setup`.
- `src/captive_portal.cpp` — implementation.
- `PAGE_PORTAL` in `include/pages.h` — styled onboarding page.

### Modified files
- `src/web_server.cpp`
  - Remove `#include <WiFiManager.h>`.
  - Replace the portal block (current lines ~531–558, the `wifiManager.startConfigPortal` section) with `captive_portal_start(); captive_portal_run();` and handle the return/restart.
  - `/` route: when `s_wifi_config_mode` is true, serve `PAGE_PORTAL` instead of `PAGE_SHELL`.
  - Add route `POST /api/portal/setup`.
  - Add captive-detection routes: `GET /generate_204`, `GET /hotspot-detect.html`, `GET /ncsi.txt`, `GET /success.html` → HTTP 302 redirect to `/`.
  - Add `s_server.onNotFound(...)` that, when `s_wifi_config_mode`, returns 302 to `/` (so any hostname/path opens the portal).
  - `web_server_loop()` calls `captive_dns_poll()` while `s_wifi_config_mode`.
- `src/main.cpp`
  - Move `web_server_init();` to **before** `web_server_wifi_setup(false);` so the web server is listening while the portal runs. Keep the force-portal (`web_server_wifi_setup(true)`) call and the rest of `setup()` unchanged.
- `platformio.ini`
  - Remove `tzapu/WifiManager@^2.0.17` from `lib_deps` in all four environments.

### Reused (no change)
- `wifi_creds_save(ssid, pass)` — persist STA credentials to EEPROM.
- `wifi_net_save(mode, ip, gw, mask, dns)` — persist DHCP/static config.
- `mqtt_client_save_config(host, port, user, pass)` — persist MQTT config.
- `apply_wifi_static_ip()` — applied before `WiFi.begin` on connect.
- `WIFI_CONFIG_PORTAL_SSID` / `WIFI_CONFIG_PORTAL_PASS` from `config.h`.
- AP default IP `192.168.4.1` (ESP softAP default).

## Behavior / Data flow

1. `setup()` calls `web_server_init()` (server now listening), then `web_server_wifi_setup(false)`.
2. `web_server_wifi_setup`:
   - If saved creds exist and `!force_portal`: attempt connect (existing 45s blocking loop). On success return `true`.
   - Otherwise: `captive_portal_start()` then `captive_portal_run()`.
3. `captive_portal_start()`:
   - `WiFi.mode(WIFI_AP)`, `WiFi.softAP(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS)`. (STA is not needed during onboarding; the saved-creds connect path runs only after the portal completes and reboots.)
   - Start a `WiFiUDP` listener on UDP/53. For each incoming DNS query, reply with a single A record pointing to the AP IP (`WiFi.softAPIP()`), regardless of requested name.
   - Set `s_wifi_config_mode = true`, `s_wifi_config_start = millis()`.
4. `captive_portal_run()` (blocks, setup-time only — same as WiFiManager did):
   - Loop until `captive_portal_submitted()` or `millis() - s_wifi_config_start > 300000`:
     - `s_server.handleClient();`
     - `captive_dns_poll();`
     - `yield();`
   - On submit: `ESP.restart()`.
   - On timeout: `ESP.restart()` (device re-enters portal on boot).
5. `POST /api/portal/setup` JSON body:
   ```
   { "ssid": "...", "pass": "...",
     "mode": 0|1, "ip":"", "gateway":"", "subnet":"", "dns":"",
     "mqtt_host":"", "mqtt_port":1883, "mqtt_user":"", "mqtt_pass":"" }
   ```
   - Validate `ssid` non-empty (400 otherwise).
   - `wifi_creds_save(ssid, pass)`, `wifi_net_save(mode, ip, gw, mask, dns)`, `mqtt_client_save_config(...)`.
   - Set submitted flag; respond `200 {"status":"ok"}`.
6. On reboot, `web_server_wifi_setup` finds saved creds and connects (static IP applied via `apply_wifi_static_ip()`). The dashboard becomes reachable on the LAN.

## UX details

- `PAGE_PORTAL` uses the style guide CSS (`:root` variables, `.card`, `.btn-primary`, `.form-group`). Title: device ID + "Configuração Wi-Fi".
- Fields:
  - WiFi: SSID (text input + optional scanned-network dropdown via `GET /api/portal/scan`), Password, DHCP/Static segmented toggle, and static fields (IP, Gateway, Subnet, DNS) shown only when Static is selected.
  - MQTT: Broker IP, Port, User, Pass — same fields as the dashboard MQTT modal.
  - "Salvar" button → `POST /api/portal/setup`. On success show "Reiniciando...".
- `GET /api/portal/scan` → `WiFi.scanNetworks()`, return JSON `{ "networks": [ {"ssid":"..","rssi":-55,"enc":2}, ... ] }`. Populate the SSID dropdown (nice-to-have; if scan fails, the free-text input is still usable).
- Save button is disabled until SSID is filled; when Static, IP + Gateway required.

## Error handling

- Empty SSID → 400, portal stays open.
- Static mode with missing IP/Gateway → rejected client-side (disabled Save) and 400 server-side.
- Portal 300s timeout → restart → re-enters portal (no silent failure).
- No WiFiManager fallback by design.

## Testing / verification

1. `pio run -e esp32_gateway` and `-e esp8266_gateway` build clean (WiFiManager gone, no undefined refs).
2. Full erase (`pio run -e esp32_gateway -t erase`) then flash.
3. Device boots into AP `ESPNOW_Gateway_Setup`; `scan.py` / phone shows the AP.
4. Connect to AP, open `http://192.168.4.1` → styled portal loads; phone shows "sign-in required".
5. SSID scan lists nearby networks.
6. Submit WiFi (DHCP) + MQTT (192.168.1.12:1883, user `kzuca`) → device reboots and connects; `scan.py` finds it with correct IP; dashboard `/settings` shows MQTT connected.
7. Repeat with Static IP selected → after reboot device uses the fixed IP.
8. Dashboard `/settings` "Configurar Rede" still works post-connect (existing feature, unaffected).
9. Force-portal (pair button long-press path / `web_server_wifi_setup(true)`) still enters the custom portal.

## Risks / notes

- Moving `web_server_init()` earlier means the server is live during the 45s connect attempt; this is harmless (dashboard would just show a connecting state). Routes reference functions initialized later but only execute on request.
- Captive DNS is a minimal UDP responder (reply AP-IP for all queries). It must not block; polled from both `captive_portal_run()` and `web_server_loop()`.
- Flash is near the 1 MB app-partition limit (~99%); removing WiFiManager should free space, but keep an eye on size after the change.
