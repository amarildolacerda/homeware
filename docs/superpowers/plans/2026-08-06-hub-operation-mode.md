# Hub Operation Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 3 operation modes (Terminal/AP/Hybrid) to the hub, persisted in EEPROM, switchable via console and API.

**Architecture:** Single new EEPROM byte stores the mode. `web_server_wifi_setup()` branches on mode: Terminal = current flow, AP = pure softAP, Hybrid = WIFI_AP_STA. Watchdog and MQTT are conditionally disabled in AP mode. Console `m` command and `/api/config/mode` endpoint enable switching.

**Tech Stack:** Arduino C++, ESP8266/ESP32 WiFi, EEPROM, ArduinoJson

## Global Constraints

- Device ID: `agri_<chip_id>` (dynamic, rule 1)
- `device_name[32]`: max 32 bytes, rule 17
- EEPROM_SIZE must accommodate new offset without exceeding 512 (shared limit)
- Loop non-blocking: no `delay()` >100ms in loop (rule 15)
- WiFi reconnect is non-blocking (rule 26)
- Branch: `dev` only; no auto-push to origin (AGENTS.md)
- FW_VERSION must be consistent across hub/server/nodes after tagging (rule 13)

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `hub/include/config.h` | Modify | Add EEPROM offset, mode defines, AP constants |
| `hub/src/web_server.cpp` | Modify | Add `op_mode_load/save`, branch `web_server_wifi_setup`, modify `web_server_maintain_wifi` |
| `hub/include/web_server.h` | Modify | Declare `op_mode_load`, `op_mode_save` |
| `hub/src/main.cpp` | Modify | Console `m` command, watchdog/MQTT conditional, status display |

---

### Task 1: EEPROM defines and mode constants in config.h

**Files:**
- Modify: `hub/include/config.h:61-66`

- [ ] **Step 1: Add EEPROM offset and mode defines**

After line 61 (`EEPROM_PAIRING_EN_OFFSET`), add:

```cpp
#define EEPROM_OP_MODE_OFFSET (EEPROM_PAIRING_EN_OFFSET + 1)
#define EEPROM_SIZE (EEPROM_OP_MODE_OFFSET + 1)

// Operation modes
#define OP_MODE_TERMINAL 0
#define OP_MODE_AP       1
#define OP_MODE_HYBRID   2
#define OP_MODE_DEFAULT  OP_MODE_TERMINAL

// AP operational config
#define AP_CHANNEL       1
#define AP_SSID_PREFIX   PLATFORM_PREFIX "_gateway"
#define AP_PASS          "password123"
```

Remove the old `EEPROM_SIZE` on line 62 (it's now defined above). Remove the old `WIFI_CONFIG_PORTAL_SSID` and `WIFI_CONFIG_PORTAL_PASS` on lines 64-65 since they're replaced by `AP_SSID_PREFIX` and `AP_PASS`. Actually, keep `WIFI_CONFIG_PORTAL_SSID` and `WIFI_CONFIG_PORTAL_PASS` — they're used by `captive_portal.cpp` for the *config portal* AP (different from the *operational* AP). So the final result is:

```cpp
#define EEPROM_PAIRING_EN_OFFSET (EEPROM_WIFI_DNS_OFFSET + EEPROM_WIFI_DNS_SIZE)
#define EEPROM_OP_MODE_OFFSET (EEPROM_PAIRING_EN_OFFSET + 1)
#define EEPROM_SIZE (EEPROM_OP_MODE_OFFSET + 1)

#define WIFI_CONFIG_PORTAL_SSID "ESPNOW_Gateway_Setup"
#define WIFI_CONFIG_PORTAL_PASS "password123"

// Operation modes
#define OP_MODE_TERMINAL 0
#define OP_MODE_AP       1
#define OP_MODE_HYBRID   2
#define OP_MODE_DEFAULT  OP_MODE_TERMINAL

// Operational AP config (modes 1 and 2)
#define AP_CHANNEL 1
#define AP_PASS    "password123"
```

- [ ] **Step 2: Verify EEPROM_SIZE fits in 512 bytes**

Run: `grep -n "EEPROM_SIZE" hub/include/config.h`
Expected: Two lines — `EEPROM_OP_MODE_OFFSET + 1` definition. Value should be ≤ 512.

- [ ] **Step 3: Commit**

```bash
git add hub/include/config.h
git commit -m "feat(hub): add operation mode EEPROM defines and AP constants"
```

---

### Task 2: op_mode_load/save in web_server.cpp

**Files:**
- Modify: `hub/src/web_server.cpp:130-148` (after `pairing_config_save`)
- Modify: `hub/include/web_server.h`

**Interfaces:**
- Produces: `int op_mode_load()`, `void op_mode_save(int mode)` — used by `web_server_wifi_setup`, `web_server_maintain_wifi`, `main.cpp`

- [ ] **Step 1: Add op_mode_load and op_mode_save after pairing_config_save**

After `pairing_config_save()` (line 148), add:

```cpp
int op_mode_load() {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_OP_MODE_OFFSET);
    EEPROM.end();
    if (val <= OP_MODE_HYBRID) return val;
    // Invalid: write default
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_OP_MODE_OFFSET, OP_MODE_DEFAULT);
    EEPROM.commit();
    EEPROM.end();
    return OP_MODE_DEFAULT;
}

void op_mode_save(int mode) {
    if (mode < OP_MODE_TERMINAL || mode > OP_MODE_HYBRID) mode = OP_MODE_DEFAULT;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_OP_MODE_OFFSET, mode);
    EEPROM.commit();
    EEPROM.end();
    console.printf("[MODE] Modo de operacao salvo: %d (%s)\n", mode,
        mode == OP_MODE_TERMINAL ? "Terminal" :
        mode == OP_MODE_AP ? "Ponto de Acesso" : "Hibrido");
}
```

- [ ] **Step 2: Add declarations to web_server.h**

In `hub/include/web_server.h`, add before `#endif`:

```cpp
int op_mode_load();
void op_mode_save(int mode);
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/web_server.cpp hub/include/web_server.h
git commit -m "feat(hub): add op_mode_load/save EEPROM functions"
```

---

### Task 3: Modify web_server_wifi_setup for operation modes

**Files:**
- Modify: `hub/src/web_server.cpp:718-775` (web_server_wifi_setup function)

**Interfaces:**
- Consumes: `op_mode_load()` from Task 2

- [ ] **Step 1: Replace web_server_wifi_setup function**

Replace the entire `web_server_wifi_setup` function (lines 718-775) with:

```cpp
bool web_server_wifi_setup(bool force_portal) {
    int op_mode = op_mode_load();
    console.printf("[WIFI] Operation mode: %d (%s)\n", op_mode,
        op_mode == OP_MODE_TERMINAL ? "Terminal" :
        op_mode == OP_MODE_AP ? "AP" : "Hibrido");

    /* --- Force portal always opens captive portal --- */
    if (force_portal) {
        console.println("[WIFI] Forcing config portal...");
        s_wifi_config_mode = true;
        s_wifi_config_start = millis();
        captive_portal_start();
        web_server_init();
        captive_portal_run();
        s_wifi_config_mode = false;
        return false;
    }

    /* --- Mode 1: Pure AP --- */
    if (op_mode == OP_MODE_AP) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "%s", get_gateway_device_id());
        console.printf("[WIFI] Starting operational AP: %s ch=%d\n", dev_name, AP_CHANNEL);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(dev_name, AP_PASS, AP_CHANNEL);
        console.printf("[WIFI] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        s_wifi_config_mode = false;
        web_server_init();
        return true;
    }

    /* --- Mode 2: Hybrid (AP + STA) --- */
    if (op_mode == OP_MODE_HYBRID) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "%s", get_gateway_device_id());
        console.printf("[WIFI] Starting hybrid AP: %s ch=%d\n", dev_name, AP_CHANNEL);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(dev_name, AP_PASS, AP_CHANNEL);
        console.printf("[WIFI] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        // Fall through to STA connect below
    }

    /* --- Mode 0 (Terminal) and Mode 2 (Hybrid STA part): try STA --- */
    char saved_ssid[EEPROM_WIFI_SSID_SIZE];
    char saved_pass[EEPROM_WIFI_PASS_SIZE];
    bool have_creds = wifi_creds_load(saved_ssid, saved_pass);

    /* --- Step 1: Try saved credentials from EEPROM --- */
    if (have_creds) {
        console.printf("[WIFI] Step 1: Connecting to saved (EEPROM): %s\n", saved_ssid);
        if (op_mode == OP_MODE_TERMINAL) WiFi.mode(WIFI_STA);
        // In hybrid, WIFI_AP_STA is already set
        apply_wifi_static_ip();
        WiFi.begin(saved_ssid, saved_pass);
        unsigned long t0 = millis();
        while (millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED) {
            delay(200);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            web_server_init();
            return true;
        }
        console.println("[WIFI] Step 1 failed: saved WiFi");
    }

#ifdef STATIC_WIFI
    /* --- Step 2: Try STATIC_WIFI hardcoded credentials --- */
    if (strlen(WIFI_SSID) > 0) {
        console.printf("[WIFI] Step 2: Trying STATIC_WIFI: %s\n", WIFI_SSID);
        if (op_mode == OP_MODE_TERMINAL) WiFi.mode(WIFI_STA);
        apply_wifi_static_ip();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long t0 = millis();
        while (millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED) {
            delay(200);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[WIFI] Connected via STATIC_WIFI! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            web_server_init();
            return true;
        }
        console.println("[WIFI] Step 2 failed: STATIC_WIFI");
    }
#endif

    /* --- Hybrid: STA failed, but AP is already running --- */
    if (op_mode == OP_MODE_HYBRID) {
        console.println("[WIFI] Hybrid: STA connect failed, AP remains active");
        s_wifi_config_mode = false;
        web_server_init();
        return true;  // AP is running, don't restart
    }

    /* --- Terminal: no STA → start captive portal --- */
    console.println("[WIFI] Starting config portal...");
    s_wifi_config_mode = true;
    s_wifi_config_start = millis();
    captive_portal_start();
    web_server_init();
    captive_portal_run();
    s_wifi_config_mode = false;
    return false;
}
```

- [ ] **Step 2: Commit**

```bash
git add hub/src/web_server.cpp
git commit -m "feat(hub): web_server_wifi_setup branches by operation mode"
```

---

### Task 4: Modify web_server_maintain_wifi for mode awareness

**Files:**
- Modify: `hub/src/web_server.cpp:777-823` (web_server_maintain_wifi function)

**Interfaces:**
- Consumes: `op_mode_load()` from Task 2

- [ ] **Step 1: Add mode check at top of web_server_maintain_wifi**

Add at the very beginning of `web_server_maintain_wifi()` (after the function signature, line 777):

```cpp
void web_server_maintain_wifi() {
    int op_mode = op_mode_load();
    if (op_mode == OP_MODE_AP) return;  // Pure AP: no STA reconnect
    // ... rest of existing function unchanged
```

- [ ] **Step 2: Commit**

```bash
git add hub/src/web_server.cpp
git commit -m "feat(hub): skip STA reconnect in AP mode"
```

---

### Task 5: Console command 'm' and status display in main.cpp

**Files:**
- Modify: `hub/src/main.cpp:58-68` (print_help)
- Modify: `hub/src/main.cpp:112-136` (case 's' status)
- Modify: `hub/src/main.cpp:305-327` (loop watchdog)
- Modify: `hub/src/main.cpp:354-364` (loop mqtt_client_loop)
- Modify: `hub/src/main.cpp:296-302` (setup mqtt_client_connect)

**Interfaces:**
- Consumes: `op_mode_load()`, `op_mode_save()` from Task 2
- Consumes: `get_gateway_device_id()` from platform

- [ ] **Step 1: Add 'm' to print_help**

In `print_help()`, add before the `=================` closing:

```cpp
    console.printf("  m    - Modo de operacao (atual: %d)\n", op_mode_load());
    console.println("  m 0  - Terminal (conecta ao roteador)");
    console.println("  m 1  - Ponto de Acesso (AP isolado)");
    console.println("  m 2  - Hibrido (AP + STA)");
```

- [ ] **Step 2: Add 'm' handler in handle_console**

After the `case 'w'` block (line 143), add:

```cpp
        case 'm':
        case 'M': {
            int cur = op_mode_load();
            // Check if next char is a digit for mode selection
            int next = -1;
            if (Serial.available() > 0) {
                char nc = Serial.peek();
                if (nc >= '0' && nc <= '2') {
                    next = nc - '0';
                    Serial.read(); // consume the digit
                }
            }
            if (next >= 0) {
                if (next == cur) {
                    console.printf("Modo ja e %d (%s)\n", cur,
                        cur == 0 ? "Terminal" : cur == 1 ? "AP" : "Hibrido");
                } else {
                    op_mode_save(next);
                    console.printf("Modo alterado: %d -> %d (%s). Reiniciando...\n",
                        cur, next,
                        next == 0 ? "Terminal" : next == 1 ? "AP" : "Hibrido");
                    delay(300);
                    ESP.restart();
                }
            } else {
                console.printf("Modo atual: %d (%s)\n", cur,
                    cur == 0 ? "Terminal" : cur == 1 ? "AP" : "Hibrido");
                console.println("  m 0 - Terminal | m 1 - AP | m 2 - Hibrido");
            }
            break;
        }
```

- [ ] **Step 3: Add op_mode to 's' status display**

In the `case 's'` block, add after the WiFi line (after line 133):

```cpp
            int op = op_mode_load();
            console.printf("Modo: %d (%s)\n", op,
                op == 0 ? "Terminal" : op == 1 ? "AP" : "Hibrido");
```

- [ ] **Step 4: Conditionally disable watchdog in AP mode**

Wrap the watchdog block (lines 311-327) with a mode check:

```cpp
    // Watchdog: only in Terminal and Hybrid (with STA connected)
    int cur_op_mode = op_mode_load();
    if (cur_op_mode != OP_MODE_AP && sensor_registry_count_paired() > 0) {
        // ... existing watchdog code unchanged ...
    }
```

- [ ] **Step 5: Conditionally disable MQTT in AP mode**

In `setup()`, wrap `mqtt_client_connect()` (line 296):

```cpp
    if (op_mode_load() != OP_MODE_AP) {
        mqtt_client_connect();
    }
```

In `loop()`, wrap `mqtt_client_loop()` (line 364):

```cpp
    if (op_mode_load() != OP_MODE_AP) {
        mqtt_client_loop();
    }
```

- [ ] **Step 6: Show mode in startup banner**

After the device ID print (line 241), add:

```cpp
    console.printf("  Mode: %d (%s)\n", op_mode_load(),
        op_mode_load() == 0 ? "Terminal" : op_mode_load() == 1 ? "AP" : "Hibrido");
```

- [ ] **Step 7: Commit**

```bash
git add hub/src/main.cpp
git commit -m "feat(hub): console mode command, conditional watchdog/mqtt by mode"
```

---

### Task 6: API endpoint for operation mode

**Files:**
- Modify: `hub/src/web_server.cpp:458-550` (after /api/config/pairing endpoints)

**Interfaces:**
- Consumes: `op_mode_load()`, `op_mode_save()` from Task 2

- [ ] **Step 1: Add GET and POST /api/config/mode endpoints**

After the `/api/config/pairing` POST handler (line 550), add:

```cpp
    s_server.on("/api/config/mode", HTTP_GET, []() {
        JsonDocument doc;
        doc["mode"] = op_mode_load();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/config/mode", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("mode")) {
            s_server.send(400, "application/json", "{\"error\":\"mode required\"}");
            return;
        }
        int mode = doc["mode"];
        if (mode < OP_MODE_TERMINAL || mode > OP_MODE_HYBRID) {
            s_server.send(400, "application/json", "{\"error\":\"invalid mode (0-2)\"}");
            return;
        }
        int cur = op_mode_load();
        if (mode == cur) {
            s_server.send(200, "application/json", "{\"status\":\"no change\"}");
            return;
        }
        op_mode_save(mode);
        s_server.send(200, "application/json", "{\"status\":\"ok\",\"restarting\":true}");
        delay(300);
        ESP.restart();
    });
```

- [ ] **Step 2: Add op_mode to /api/info response**

In the `/api/info` handler (around line 240), add:

```cpp
        doc["op_mode"] = op_mode_load();
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/web_server.cpp
git commit -m "feat(hub): add /api/config/mode GET/POST endpoints"
```

---

### Task 7: Build verification

**Files:** None (verification only)

- [ ] **Step 1: Build for ESP32**

Run: `cd hub && pio run -e esp32`
Expected: BUILD SUCCESSFUL

- [ ] **Step 2: Build for ESP8266**

Run: `cd hub && pio run -e d1_mini`
Expected: BUILD SUCCESSFUL (verify EEPROM_SIZE ≤ 512, no memory overflow)

- [ ] **Step 3: Final commit if any fixups needed**

```bash
git add -A
git commit -m "fix(hub): operation mode build fixes"
```
