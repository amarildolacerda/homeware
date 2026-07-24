# Sensor de Humidade do Solo — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Criar cliente ESP8266 sensor de humidade do solo com deep sleep, mais suporte no gateway e bridge.

**Architecture:** Deep sleep puro — acorda, conecta WiFi, init ESP-NOW, lê ADC, envia broadcast, dorme. Sem web server runtime. Configuração via WiFiManager no boot (2 min). Só envia dados se conhecer o gateway MAC.

**Tech Stack:** PlatformIO (ESP8266), ESP-NOW broadcast, WiFiManager, deep sleep

## Global Constraints

- `SENSOR_TYPE_SOIL_MOISTURE = 12` (shared protocol)
- `payload_soil_moisture_t { uint16_t raw_adc; uint8_t moisture_pct; }`
- Broadcast obrigatório (ESP8266→ESP32, regra 18)
- `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes de `esp_now_init` (regra 21)
- Deep sleep via `ESP.deepSleep()`
- Gateway MAC salvo em EEPROM; só envia dados se MAC conhecido
- `FW_VERSION` = tag atual
- `lib_extra_dirs = ../../shared` (nunca copiar shared)

---
### Task 1: Add sensor type + payload to shared protocol

**Files:**
- Modify: `shared/src/espnow_protocol.h`

- [ ] **Add SENSOR_TYPE_SOIL_MOISTURE to enum**

```c
SENSOR_TYPE_SOIL_MOISTURE = 12,
```
Add after line 48 (`SENSOR_TYPE_DHT_RELE = 11,`)

- [ ] **Add payload struct**

```c
typedef struct __attribute__((packed)) {
    uint16_t raw_adc;
    uint8_t moisture_pct;
} payload_soil_moisture_t;
```
Add after `payload_onoff_t` (after line 116)

- [ ] **Commit**

```bash
git add shared/src/espnow_protocol.h
git commit -m "feat: add SENSOR_TYPE_SOIL_MOISTURE + payload_soil_moisture_t"
```

---
### Task 2: Gateway — sensor_registry handling

**Files:**
- Modify: `gateway/include/sensor_registry.h` (state union)
- Modify: `gateway/src/sensor_registry.cpp` (5 locations)

- [ ] **Add state sub-struct to virtual_sensor_t union**

In `gateway/include/sensor_registry.h`, after line 28 (`uint8_t rain_digital;`), add:
```c
struct {
    uint16_t raw_adc;
    uint8_t moisture_pct;
} soil_moisture;
```

- [ ] **Add case in sensor_registry_update_state**

In `gateway/src/sensor_registry.cpp`, in the payload switch (~line 190) add:
```c
case SENSOR_TYPE_SOIL_MOISTURE:
{
    if (len < sizeof(payload_soil_moisture_t)) return;
    payload_soil_moisture_t *pl = (payload_soil_moisture_t *)payload;
    s->state.soil_moisture.raw_adc = pl->raw_adc;
    s->state.soil_moisture.moisture_pct = pl->moisture_pct;
    break;
}
```

- [ ] **Add expected size case**

In the expected size switch (~line 215), add:
```c
case SENSOR_TYPE_SOIL_MOISTURE:
    expected = sizeof(payload_soil_moisture_t);
    break;
```

- [ ] **Update EEPROM validation boundary**

In `sensor_registry_load()` (~line 269), change `SENSOR_TYPE_REPEATER` to `SENSOR_TYPE_SOIL_MOISTURE`:
```c
if (type < SENSOR_TYPE_TEMP_HUM || type > SENSOR_TYPE_SOIL_MOISTURE ||
```

- [ ] **Add friendly_name entry** (~line 339):
```c
case SENSOR_TYPE_SOIL_MOISTURE: return "Solo";
```

- [ ] **Add to_string entry** (~line 355):
```c
case SENSOR_TYPE_SOIL_MOISTURE: return "soil_moisture";
```

- [ ] **Commit**

```bash
git add gateway/include/sensor_registry.h gateway/src/sensor_registry.cpp
git commit -m "feat: add soil_moisture sensor type to gateway registry"
```

---
### Task 3: Gateway — web_server + mqtt_client

**Files:**
- Modify: `gateway/src/web_server.cpp`
- Modify: `gateway/src/mqtt_client.cpp`

- [ ] **Add JSON state mapping in web_server**

In `gateway/src/web_server.cpp`, in the `/api/sensors` switch (~line 315), add before `default`:
```c
case SENSOR_TYPE_SOIL_MOISTURE:
    obj["moisture_pct"] = s->state.soil_moisture.moisture_pct;
    obj["raw_adc"] = s->state.soil_moisture.raw_adc;
    break;
```

- [ ] **Add MQTT discovery entry**

In `gateway/src/mqtt_client.cpp`, in `mqtt_client_publish_discovery` (add before `SENSOR_TYPE_TANK` at ~line 368), add:
```c
case SENSOR_TYPE_SOIL_MOISTURE:
{
    build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "soil");
    publish_entity_config("sensor", entity, name, "Humidade Solo", "humidity", "%", false, id, model);
    publish_entity_state("sensor", entity,
                         String(sensor->state.soil_moisture.moisture_pct).c_str());
    break;
}
```

- [ ] **Add MQTT state publishing**

In `mqtt_client.cpp`, in `mqtt_client_publish_state` (add before `SENSOR_TYPE_TANK` at ~line 468), add:
```c
case SENSOR_TYPE_SOIL_MOISTURE:
{
    char val[16];
    build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "soil");
    snprintf(val, sizeof(val), "%u", sensor->state.soil_moisture.moisture_pct);
    publish_entity_state("sensor", entity, val);
    break;
}
```

- [ ] **Commit**

```bash
git add gateway/src/web_server.cpp gateway/src/mqtt_client.cpp
git commit -m "feat: add soil_moisture API + MQTT support in gateway"
```

---
### Task 4: Bridge — add soil_moisture type + MQTT discovery

**Files:**
- Modify: `bridge/bridge_python/app/models.py`
- Modify: `bridge/bridge_python/app/mqtt_discovery.py`

- [ ] **Add DeviceType enum member**

In `bridge/bridge_python/app/models.py`, after line 17 (`RAIN = "rain"`):
```python
SOIL_MOISTURE = "soil_moisture"
```

- [ ] **Add MQTT entity mapping**

In `bridge/bridge_python/app/mqtt_discovery.py`, in `DEVICE_ENTITY_MAP` (~line 31), add after `RAIN`:
```python
DeviceType.SOIL_MOISTURE: [
    ("moisture_pct", "sensor", "%", "humidity", "mdi:water-percent"),
],
```

- [ ] **Commit**

```bash
git add bridge/bridge_python/app/models.py bridge/bridge_python/app/mqtt_discovery.py
git commit -m "feat: add soil_moisture type to bridge"
```

---
### Task 5: Client — project structure + platformio

**Files:**
- Create: `clients/esp8266_soil_moisture/platformio.ini`
- Create: `clients/esp8266_soil_moisture/.gitignore`
- Create: `clients/esp8266_soil_moisture/build.sh`
- Create: `clients/esp8266_soil_moisture/flash.sh`
- Create: `clients/esp8266_soil_moisture/flash.ps1`
- Create: `clients/esp8266_soil_moisture/monitor.sh`

- [ ] **Create directory**

```bash
mkdir -p clients/esp8266_soil_moisture/include clients/esp8266_soil_moisture/src
```

- [ ] **platformio.ini**

```
[env:esp8266]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
monitor_filters = esp8266_exception_decoder
lib_extra_dirs =
    ../../shared
build_flags =
    -D ESPNOW_CHANNEL=1
    -I../../shared
    -I../../shared/src
lib_deps =
    bblanchon/ArduinoJson@^7.3.1
    tzapu/WiFiManager@^2.0.16

[env:esp8266_ota]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_protocol = espota
upload_port = 192.168.1.53
lib_extra_dirs =
    ../../shared
build_flags =
    -D ESPNOW_CHANNEL=1
    -I../../shared
    -I../../shared/src
lib_deps =
    bblanchon/ArduinoJson@^7.3.1
    tzapu/WiFiManager@^2.0.16
```

- [ ] **.gitignore**

```
.pio/
.vscode/
build/
```

- [ ] **build.sh**

```bash
#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
pio run
```

- [ ] **flash.sh**

```bash
#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
PORT_ARG=""
if [ -n "$1" ]; then
    PORT_ARG="--upload-port $1"
fi
pio run --target upload $PORT_ARG
```

- [ ] **flash.ps1**

```powershell
param(
    [string]$Port
)
if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
    Write-Error "PlatformIO (pio) not found"
    exit 1
}
$portArg = ""
if ($Port) {
    $portArg = "--upload-port $Port"
}
pio run --target upload $portArg
```

- [ ] **monitor.sh**

```bash
#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
pio device monitor
```

```bash
chmod +x clients/esp8266_soil_moisture/*.sh
```

- [ ] **Commit**

```bash
git add clients/esp8266_soil_moisture/
git commit -m "feat: scaffold soil_moisture client project"
```

---
### Task 6: Client — config.h

**Files:**
- Create: `clients/esp8266_soil_moisture/include/config.h`

```c
#pragma once
#include <Arduino.h>

#define DEVICE_NAME "Soil Moisture"

#define STATE_UPDATE_INTERVAL 5000
#define HEARTBEAT_INTERVAL 60000

#define SOIL_ADC_PIN A0
#define SOIL_CFG_BUTTON_PIN 0
#define LED_PIN 2

#define LED_BLINK_SEND_MS 100

#define ESPNOW_CHANNEL 1
#define ESPNOW_PAIR_INTERVAL_MS 3000
#define ESPNOW_MAX_PAIR_ATTEMPTS 10
#define ESPNOW_PAIR_TIMEOUT_MS 30000

#define WIFI_CONFIG_PORTAL_SSID "ESP8266_SoilMoisture"
#define WIFI_CONFIG_PORTAL_PASS "password123"
#define WIFI_CONFIG_PORTAL_TIMEOUT_S 120
#define WIFI_CONNECT_TIMEOUT_MS 5000

#define DEEP_SLEEP_INTERVAL_DEFAULT 600
#define DEEP_SLEEP_INTERVAL_MIN 60
#define DEEP_SLEEP_INTERVAL_MAX 3600
```

- [ ] **Commit**

```bash
git add clients/esp8266_soil_moisture/include/config.h
git commit -m "feat: add soil_moisture config.h"
```

---
### Task 7: Client — main.cpp

**Files:**
- Create: `clients/esp8266_soil_moisture/src/main.cpp`

```c
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <espnow.h>
#include "config.h"
#include "espnow_protocol.h"
#include "common_console.h"
#include "common_espnow.h"

static const char *TAG = "esp8266-soil";

static bool s_espnow_ready = false;
static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;
static uint16_t s_sequence = 0;

static uint8_t s_gateway_mac[6];
static bool s_has_gateway = false;
static uint16_t s_assigned_slot = 0;

static uint16_t s_soil_raw = 0;
static uint8_t s_soil_pct = 0;
static int s_battery = 100;

static unsigned long s_active_start = 0;
static bool s_config_mode = false;

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_INTERVAL_ADDR 60
#define EEPROM_INTERVAL_SIZE 2

static int s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

static void read_sensor(void)
{
    s_soil_raw = analogRead(SOIL_ADC_PIN);
    s_soil_pct = (uint8_t)constrain(map(s_soil_raw, 0, 1024, 0, 100), 0, 100);
    static int counter = 0;
    counter++;
    if (counter > 100) {
        counter = 0;
        s_battery = max(0, s_battery - 1);
    }
}

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    (void)status;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    if (!data || len < 1) return;
    if (data[0] == ESPNOW_MSG_PAIR_RESPONSE)
    {
        if (len < sizeof(espnow_pair_response_t)) return;
        espnow_pair_response_t *resp = (espnow_pair_response_t *)data;
        if (resp->status == PAIR_STATUS_OK)
        {
            mac_copy(s_gateway_mac, mac);
            s_assigned_slot = resp->assigned_slot;
            espnow_save_gateway_mac(mac, TAG);
            s_has_gateway = true;
            char mac_str[18];
            mac_to_str(mac, mac_str, sizeof(mac_str));
            console.printf("[%s] Paired with gateway %s slot %d\n", TAG, mac_str, s_assigned_slot);
        }
    }
}

static bool espnow_send_pair_request(void)
{
    if (!s_espnow_ready) return false;
    uint8_t buf[sizeof(espnow_pair_request_t)];
    memset(buf, 0, sizeof(buf));
    espnow_pair_request_t *req = (espnow_pair_request_t *)buf;
    req->msg_type = ESPNOW_MSG_PAIR_REQUEST;
    req->sequence = s_sequence++;
    WiFi.macAddress(req->sensor_mac);
    req->sensor_type = SENSOR_TYPE_SOIL_MOISTURE;
    uint32_t ver = 0x000A000B;
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';
    if (!espnow_client_add_peer(s_broadcast_mac, TAG)) return false;
    return espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG);
}

static bool espnow_send_data(void)
{
    if (!s_espnow_ready) return false;
    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_soil_moisture_t) + 4];
    memset(buf, 0, sizeof(buf));
    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_SOIL_MOISTURE;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();
    payload_soil_moisture_t *pl = (payload_soil_moisture_t *)hdr->payload;
    pl->raw_adc = s_soil_raw;
    pl->moisture_pct = s_soil_pct;
    IPAddress ip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_soil_moisture_t);
    ip_ptr[0] = ip[0]; ip_ptr[1] = ip[1]; ip_ptr[2] = ip[2]; ip_ptr[3] = ip[3];
    hdr->payload_len = sizeof(payload_soil_moisture_t) + 4;
    if (!espnow_client_add_peer(s_broadcast_mac, TAG)) return false;
    return espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG);
}

static void do_deep_sleep(void)
{
    console.printf("[%s] Deep sleep %ds\n", TAG, s_interval_s);
    Serial.flush();
    ESP.deepSleep((uint64_t)s_interval_s * 1000000ULL, WAKE_RF_DEFAULT);
}

static bool try_pairing(void)
{
    console.printf("[%s] Starting pairing (max %d attempts)...\n", TAG, ESPNOW_MAX_PAIR_ATTEMPTS);
    for (int i = 0; i < ESPNOW_MAX_PAIR_ATTEMPTS; i++)
    {
        espnow_send_pair_request();
        unsigned long deadline = millis() + ESPNOW_PAIR_INTERVAL_MS;
        while (millis() < deadline)
        {
            console.loop();
            if (s_has_gateway) return true;
            yield();
        }
        console.printf("[%s] Pair attempt %d/%d\n", TAG, i + 1, ESPNOW_MAX_PAIR_ATTEMPTS);
        if (millis() - s_active_start > (unsigned long)ESPNOW_PAIR_TIMEOUT_MS)
        {
            console.printf("[%s] Pairing timeout\n", TAG);
            break;
        }
    }
    return false;
}

static void init_hardware(void)
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    pinMode(SOIL_CFG_BUTTON_PIN, INPUT_PULLUP);
}

static bool wifi_setup(bool force_config_portal)
{
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20);
    if (!force_config_portal && WiFi.SSID() != "")
    {
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(1);
        console.printf("[%s] Connecting to saved WiFi: %s\n", TAG, WiFi.SSID().c_str());
        if (wifiManager.autoConnect())
        {
            console.printf("[%s] WiFi connected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            return true;
        }
        console.printf("[%s] Failed to connect to saved WiFi\n", TAG);
    }
    console.printf("[%s] Starting config portal (%ds)...\n", TAG, WIFI_CONFIG_PORTAL_TIMEOUT_S);
    s_config_mode = true;
    wifiManager.setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT_S);
    char interval_str[8];
    snprintf(interval_str, sizeof(interval_str), "%d", s_interval_s / 60);
    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 32);
    WiFiManagerParameter custom_interval("interval", "Interval (min)", interval_str, 4);
    wifiManager.addParameter(&custom_dev_name);
    wifiManager.addParameter(&custom_interval);
    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0)
        {
            strncpy(s_device_name, custom_dev_name.getValue(), sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
            espnow_save_device_name(s_device_name);
        }
        int new_interval = atoi(custom_interval.getValue()) * 60;
        if (new_interval >= DEEP_SLEEP_INTERVAL_MIN && new_interval <= DEEP_SLEEP_INTERVAL_MAX)
        {
            s_interval_s = new_interval;
            EEPROM.put(EEPROM_INTERVAL_ADDR, s_interval_s);
            EEPROM.commit();
        }
        s_config_mode = false;
        return true;
    }
    s_config_mode = false;
    return false;
}

void setup(void)
{
    Serial.begin(115200);
    delay(500);
    console.begin();
    s_active_start = millis();

    uint32_t chip_id = ESP.getChipId();
    snprintf(s_device_id, sizeof(s_device_id), "esp8266_%06x", chip_id);
    espnow_load_device_name(s_device_name, sizeof(s_device_name));
    EEPROM.get(EEPROM_INTERVAL_ADDR, s_interval_s);
    if (s_interval_s < DEEP_SLEEP_INTERVAL_MIN || s_interval_s > DEEP_SLEEP_INTERVAL_MAX)
        s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

    console.printf("\n============================================\n");
    console.printf("  ESP8266 Soil Moisture " FW_VERSION "\n");
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n\n");

    randomSeed(analogRead(A0));
    init_hardware();

    bool button_pressed = (digitalRead(SOIL_CFG_BUTTON_PIN) == LOW);
    s_has_gateway = espnow_load_gateway_mac(s_gateway_mac, TAG);

    if (button_pressed || !s_has_gateway)
    {
        if (!wifi_setup(true))
        {
            console.printf("[%s] Config portal closed, sleeping\n", TAG);
            do_deep_sleep();
            return;
        }
        s_espnow_ready = espnow_client_init(TAG);
        if (s_espnow_ready)
        {
            esp_now_register_send_cb(espnow_send_cb);
            esp_now_register_recv_cb(espnow_recv_cb);
            if (!s_has_gateway)
            {
                if (try_pairing())
                {
                    console.printf("[%s] Paired successfully!\n", TAG);
                }
                else
                {
                    console.printf("[%s] Pairing failed, sleeping\n", TAG);
                    do_deep_sleep();
                    return;
                }
            }
        }
        read_sensor();
        if (s_has_gateway)
        {
            digitalWrite(LED_PIN, LOW);
            espnow_send_data();
            delay(LED_BLINK_SEND_MS);
            digitalWrite(LED_PIN, HIGH);
        }
        do_deep_sleep();
        return;
    }

    if (!s_has_gateway)
    {
        console.printf("[%s] No gateway MAC saved, sleeping\n", TAG);
        do_deep_sleep();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.begin();
    unsigned long wifi_deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < wifi_deadline)
    {
        delay(10);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        console.printf("[%s] WiFi timeout, sleeping\n", TAG);
        do_deep_sleep();
        return;
    }
    console.printf("[%s] WiFi OK, IP: %s\n", TAG, WiFi.localIP().toString().c_str());

    s_espnow_ready = espnow_client_init(TAG);
    if (s_espnow_ready)
    {
        esp_now_register_send_cb(espnow_send_cb);
        esp_now_register_recv_cb(espnow_recv_cb);
    }

    read_sensor();
    digitalWrite(LED_PIN, LOW);
    espnow_send_data();
    delay(LED_BLINK_SEND_MS);
    digitalWrite(LED_PIN, HIGH);

    do_deep_sleep();
}

void loop(void)
{
}
```

- [ ] **Commit**

```bash
git add clients/esp8266_soil_moisture/src/main.cpp
git commit -m "feat: implement soil_moisture client with deep sleep + pairing"
```

---
### Task 8: Client — SPEC.md + README.md

**Files:**
- Create: `clients/esp8266_soil_moisture/SPEC.md`
- Create: `clients/esp8266_soil_moisture/README.md`

- [ ] **SPEC.md**

```markdown
# Spec: ESP8266 Soil Moisture Sensor

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) sensor capacitivo de humidade do solo.
Comunica com o Gateway via ESP-NOW. Alimentado por bateria com deep sleep.

## Fluxo de Inicialização

1. **Hardware**: init ADC (A0), LED (GPIO2), botão config (GPIO0)
2. **WiFi**: se botão pressionado ou sem gateway MAC → portal WiFiManager (2 min)
   - Configura: SSID, senha, device name, intervalo (min)
3. **Pareamento**: após portal, envia PAIR_REQUEST a cada 3s (máx 30s)
4. **Normal**: WiFi on → ESP-NOW → lê ADC → envia dados → deep sleep

## Regras

1. Só envia dados se gateway MAC conhecido (salvo em EEPROM)
2. Broadcast ESP8266→ESP32 (regra 18)
3. `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes de `esp_now_init` (regra 21)
4. Deep sleep `ESP.deepSleep()`, sem delay bloqueante

## Hardware

| Componente | GPIO |
|------------|------|
| Sensor capacitivo | A0 |
| LED | D4/GPIO2 |
| Botão config | D3/GPIO0 (GND = configura) |
| Wake | D0/GPIO16 → RST |

## Dependências

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16
```

- [ ] **README.md**

```markdown
# ESP8266 Soil Moisture Sensor (ESP-NOW)

Cliente ESP8266 para sensor capacitivo de humidade do solo.
Comunica com o Gateway via ESP-NOW. Bateria + deep sleep.

## Configuração

Segurar D3 (GND) ao ligar → portal WiFiManager `ESP8266_SoilMoisture`.
Configure WiFi, nome e intervalo de leitura.

## Hardware

| Componente | GPIO |
|------------|------|
| Sensor (analógico) | A0 |
| LED | D4/GPIO2 |
| Botão config | D3/GPIO0 (GND no boot) |
| Wake | D0/GPIO16 → RST |

## Build

```bash
./build.sh
./flash.sh /dev/ttyUSB0
```

## API

Sem web server em runtime (deep sleep). Configuração via WiFiManager apenas.
```

- [ ] **Commit**

```bash
git add clients/esp8266_soil_moisture/SPEC.md clients/esp8266_soil_moisture/README.md
git commit -m "docs: add SPEC.md + README.md for soil_moisture client"
```
