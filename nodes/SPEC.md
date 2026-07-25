# Template/Checklist para Criação de Novos Clients ESP8266

## 1. Estrutura de Diretórios

```
clients/esp8266_<tipo>/
├── .gitignore
├── build.sh
├── flash.sh
├── flash.ps1
├── monitor.sh
├── erase.ps1              (recomendado)
├── platformio.ini
├── SPEC.md
├── README.md
├── include/
│   ├── config.h
│   └── pages.h
└── src/
    └── main.cpp
```

## 2. Checklist de Implementação

### Documentação
- [ ] `SPEC.md` — especificação funcional detalhada (fluxo, hardware, API, loop, regras)
- [ ] `README.md` — documentação do usuário (HW wiring, API table, serial commands, build)

### Código — Config
- [ ] `config.h` com defines padrão: `DEVICE_NAME`, `STATE_UPDATE_INTERVAL`, `HEARTBEAT_INTERVAL`, pinos, `LED_BLINK_WIFI_MS`, `LED_BLINK_GATEWAY_MS`, `ESPNOW_*`, `WIFI_CONFIG_PORTAL_SSID`, `WIFI_CONFIG_PORTAL_PASS`

### Código — pages.h (PROGMEM)
- [ ] `PAGE_DASHBOARD` — layout compacto: controle/badge principal + seção "Detalhes" collapsível
- [ ] `PAGE_DOCS` — documentação da API
- [ ] Página enxuta (<8KB) para usar `send_P()` sem risco (regra 16)

### Código — main.cpp
- [ ] `FW_VERSION` de `shared_config.h`
- [ ] `common_console.h` — telnet
- [ ] `common_espnow.h` — `espnow_client_init()`, save/load gateway MAC, save/load device name
- [ ] `common_web.h` — `serve_pgm_page()`
- [ ] `espnow_protocol.h` — structs e tipos do protocolo ESP-NOW

#### ESP-NOW
- [ ] `espnow_client_init(TAG)` com `WIFI_NONE_SLEEP` (regra 21)
- [ ] `espnow_send_cb` e `espnow_recv_cb`
- [ ] PAIR_REQUEST: broadcast `FF:FF:FF:FF:FF:FF` (regra 18 — ESP8266→ESP32)
- [ ] SENSOR_DATA: broadcast se target for ESP32 (regra 18)
- [ ] HEARTBEAT: broadcast se target for ESP32
- [ ] ACK/NAK handling no recv_cb
- [ ] RESTART command handling com `mac_equal(target_mac, s_my_mac)`
- [ ] State machine para send: `SEND_IDLE → SEND_WAIT_ACK → SEND_RETRY_DELAY → SEND_RETRY_WAIT_ACK`

#### Web Server
- [ ] `GET /` — dashboard
- [ ] `GET /docs` — API docs
- [ ] `GET /api/state` — JSON completo
- [ ] `GET /api/settings` + `POST /api/settings` — device name
- [ ] `GET /api/pin?gpio=N` + `POST /api/pin`
- [ ] `POST /api/ota` — firmware update (multipart)
- [ ] `POST /api/restart` — reiniciar

#### Serial Console
- [ ] `h`/`?` — help
- [ ] `s` — status
- [ ] `l` — leitura forçada + envio
- [ ] `p` — reset pareamento
- [ ] `u` — info OTA (hostname, port, commands)
- [ ] `r` — restart

#### Loop (non-blocking)
- [ ] Nenhum `delay()` bloqueante (regra 15)
- [ ] `console.loop()` + telnet + serial
- [ ] `ArduinoOTA.handle()`
- [ ] `s_server.handleClient()`
- [ ] `maintain_wifi_connection()` — sem return precoce
- [ ] `check_config_portal_timeout()`
- [ ] Pareamento: retry a cada `ESPNOW_PAIR_INTERVAL_MS`, max `ESPNOW_MAX_PAIR_ATTEMPTS`, cooldown 60s
- [ ] Send state machine: ACK wait + retries (`ESPNOW_SEND_RETRIES`)
- [ ] Heartbeat a cada `HEARTBEAT_INTERVAL`
- [ ] LED: config portal ON, WiFi blink, unparied blink, pareado OFF

### Setup
- [ ] Serial 115200 + console.begin()
- [ ] device_id = `esp8266_%06x` (chip_id)
- [ ] Carregar device_name da EEPROM
- [ ] WiFiManager com campo device_name
- [ ] `randomSeed(analogRead(A0))`
- [ ] `init_hardware()` — pinMode
- [ ] `espnow_client_init(TAG)`
- [ ] Registrar send/recv callbacks
- [ ] Rotas do servidor web
- [ ] ArduinoOTA
- [ ] Carregar gateway MAC da EEPROM
- [ ] Banner de boot

### platformio.ini
- [ ] `lib_extra_dirs = ../../shared`
- [ ] `board_build.filesystem = littlefs` (se usar SPIFFS/LittleFS)
- [ ] `build_flags` com `-I../../shared`, `-I../../shared/src`
- [ ] lib_deps: `ArduinoJson`, `WiFiManager` (mínimo)
- [ ] env `esp8266_ota` para OTA

### Scripts
- [ ] `build.sh` — `pio run`
- [ ] `flash.sh [-p <port>]` — `pio run --target upload`
- [ ] `monitor.sh` — `pio device monitor`
- [ ] `erase.ps1 [-p <port>]` — Windows erase (recomendado)

### Protocolo (shared/ + gateway)
- [ ] `SENSOR_TYPE_<TIPO>` adicionado em `shared/src/espnow_protocol.h`
- [ ] `payload_<tipo>_t` struct adicionada em `shared/src/espnow_protocol.h`
- [ ] Gateway reconhece o novo `sensor_type` no `sensor_registry`

## 3. Perguntar ao Usuário (antes de codificar)

- Deve incluir **função repeater** ESP-NOW?
- Se sim: bidirecional? Regras (broadcast do gateway, dados de clients)?

## 4. Regras Importantes (aplicar sempre)

| Regra | Descrição |
|-------|-----------|
| 1 | Device ID dinâmico (`esp8266_<chip_id>`), não configurável |
| 2 | Device name via WiFiManager, salvo EEPROM (> 32, < 127) |
| 3 | `BRIDGE_HOST = "0.0.0.0"` força discovery UDP |
| 4 | Copiar cJSON `valuestring` para buffer local antes de `cJSON_Delete` |
| 5 | Retry de registro no `loop()`, não só no `setup()` |
| 6 | `WIFI_NONE_SLEEP` antes de `esp_now_init` (regra 21) |
| 7 | LED: `LED_ON LOW`, `LED_OFF HIGH` (GPIO2) |
| 8 | `device_name[32]` em todos os structs (regra 17) |
| 9 | Broadcast ESP8266→ESP32, unicast ESP32→ESP8266 ou ESP8266→ESP8266 (regra 18) |
| 10 | Validar strings EEPROM: caracteres 0x20–0x7E antes de usar (regra 20) |
| 11 | Dashboard "Detalhes" collapsível |
| 12 | Non-blocking loop — zero `delay()` |
| 13 | `FW_VERSION` = tag atual (regra 13) |
| 14 | `lib_extra_dirs` apontando para `../../shared` (nunca copiar shared) |
