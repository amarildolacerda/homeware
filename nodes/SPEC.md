# Template/Checklist para Criação de Novos Nodes AgriSense

## 1. Estrutura de Diretórios

```
nodes/<tipo>/
├── .gitignore
├── build.sh
├── flash.sh
├── flash.ps1
├── monitor.sh
├── erase.ps1              (recomendado)
├── platformio.ini
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
- [ ] `config.h` com defines padrão: `DEVICE_NAME`, `STATE_UPDATE_INTERVAL`, `HEARTBEAT_INTERVAL`, pinos, `LED_BLINK_WIFI_MS`, `LED_BLINK_GATEWAY_MS`, `ESPNOW_*` (se ESP-NOW), `WIFI_CONFIG_PORTAL_SSID`, `WIFI_CONFIG_PORTAL_PASS`, `EEPROM_RELAY_STATE` (offset >=200)

### Código — pages.h (PROGMEM)
- [ ] `PAGE_DASHBOARD` — layout padrão: sidebar 180px (Home/WiFi/Propriedades/Config), stats-header (RX/TX/Mem/Uptime), content `max-width:480px`, footer-bar (dot gateway + clock + uptime), JS polling 3s, toggle com loading guard
- [ ] Seção "WiFi" no dashboard com status (SSID/IP/status) + formulário SSID/senha para configurar rede (obrigatório se dispositivo opera sem WiFi)
- [ ] `PAGE_DOCS` — documentação da API estilo Swagger
- [ ] `PAGE_WIFI_CONFIG` — formulário SSID/password/device_name para captive portal
- [ ] Detalhes collapsível (expande ao clicar)
- [ ] Página enxuta (<8KB) para usar `send_P()` sem risco (regra 16); se >8KB, usar `WiFiClient` chunks de 256 bytes

### Código — main.cpp
- [ ] `FW_VERSION` de `shared_config.h`
- [ ] `common_console.h` — telnet obrigatório
- [ ] Se ESP-NOW: `common_espnow.h`, `espnow_protocol.h`, `espnow_client_init()` com `WIFI_NONE_SLEEP`
- [ ] Se LoRa: `-DLORA_DEVICE` em `platformio.ini`, `lora_protocol.h`, definir `SENSOR_TYPE_ONOFF` localmente
- [ ] `myWiFiManager.h` — WiFi não-bloqueante (NUNCA `wm.autoConnect()`)

#### WiFi (não-bloqueante)
- [ ] `mywifi_begin(false)` tenta conectar com credenciais salvas
- [ ] `mywifi_loop()` + `handle_wifi()` no `loop()` monitora conexão
- [ ] Após timeout 120s, abre AP + captive portal com `DNSServer`
- [ ] Página de configuração: `PAGE_WIFI_CONFIG` via server on `/`
- [ ] `GET /api/wifi` + `POST /api/wifi` — credenciais WiFi

#### ESP-NOW (se aplicável)
- [ ] `espnow_client_init(TAG)` com `WIFI_NONE_SLEEP` (regra 21)
- [ ] `espnow_send_cb` e `espnow_recv_cb`
- [ ] PAIR_REQUEST: broadcast `FF:FF:FF:FF:FF:FF` (regra 18 — ESP8266→ESP32)
- [ ] SENSOR_DATA: broadcast se target for ESP32 (regra 18)
- [ ] HEARTBEAT: broadcast se target for ESP32
- [ ] ACK/NAK handling no recv_cb
- [ ] RESTART command handling com `mac_equal(target_mac, s_my_mac)`
- [ ] State machine para send: `SEND_IDLE → SEND_WAIT_ACK → SEND_RETRY_DELAY → SEND_RETRY_WAIT_ACK`

#### Web Server
- [ ] `GET /` — dashboard ou captive portal
- [ ] `GET /docs` — API docs
- [ ] `GET /api/state` — JSON completo (uptime_s, rx_count, tx_count, free_heap, fw_version, device_name, estado do sensor/atuador)
- [ ] `GET /api/settings` + `POST /api/settings` — device name
- [ ] `GET /api/wifi` + `POST /api/wifi` — credenciais WiFi
- [ ] `POST /api/ota` — firmware update via XHR + `Update.h`
- [ ] `POST /api/restart` — reiniciar

#### Serial Console (obrigatório)
- [ ] `h`/`?` — help
- [ ] `s` — status
- [ ] `l` — leitura forçada / listar (contextual)
- [ ] `p` — reset pareamento (se ESP-NOW)
- [ ] `r` — restart

#### Loop (non-blocking)
- [ ] Nenhum `delay()` bloqueante (regra 15)
- [ ] `console.loop()` + telnet + serial
- [ ] `ArduinoOTA.handle()`
- [ ] `s_server.handleClient()`
- [ ] `handle_wifi()` — myWiFiManager loop
- [ ] `check_config_portal_timeout()` se AP aberto
- [ ] Se ESP-NOW: pareamento retry, send state machine, heartbeat
- [ ] LED: config portal ON (sólido), WiFi blink (conectando), desligado quando OK

### Setup
- [ ] Serial 115200 + `console.begin()` + `console.set_banner()`
- [ ] device_id = `agri_%06x` (chip_id)
- [ ] Carregar device_name da EEPROM (validar imprimíveis 0x20–0x7E, regra 20)
- [ ] `mywifi_begin(false)` — WiFi não-bloqueante
- [ ] `randomSeed(analogRead(A0))`
- [ ] `init_hardware()` — pinMode
- [ ] Se ESP-NOW: `espnow_client_init(TAG)`, registrar callbacks
- [ ] Rotas do servidor web
- [ ] `ArduinoOTA.begin()`
- [ ] Banner de boot com device_id, FW_VERSION, free_heap

### platformio.ini
- [ ] `lib_extra_dirs = ../../shared`
- [ ] `board_build.filesystem = littlefs` (se usar SPIFFS/LittleFS)
- [ ] `build_flags` com `-I../../shared/src`
- [ ] Se LoRa: `-DLORA_DEVICE` em `build_flags`
- [ ] lib_deps: `ArduinoJson`, `tzapu/WiFiManager @ ^2.0` (ESP32) ou `^0.16` (ESP8266)
- [ ] env `<node>_ota` para OTA

### Scripts
- [ ] `build.sh` — `pio run`
- [ ] `flash.sh [-p <port>]` — `pio run --target upload`
- [ ] `monitor.sh` — `pio device monitor`
- [ ] `erase.ps1 [-p <port>]` — Windows erase (recomendado)

### Protocolo (shared/ + gateway)
- [ ] Se ESP-NOW: `SENSOR_TYPE_<TIPO>` + struct em `shared/src/espnow_protocol.h`, gateway reconhece no `sensor_registry`
- [ ] Se LoRa: tipos definidos localmente em `main.cpp` (ex: `#define SENSOR_TYPE_ONOFF 8`)

## 3. Perguntar ao Usuário (antes de codificar)

- Comunicação: **ESP-NOW** ou **LoRa** ou **direto via WiFi**?
- Se ESP-NOW: deve incluir **função repeater**? Bidirecional? Regras?

## 4. Regras Importantes (aplicar sempre)

| Regra | Descrição |
|-------|-----------|
| 1 | Device ID dinâmico (`agri_<chip_id>`), não configurável |
| 2 | Device name via WiFiManager, salvo EEPROM (> 32, < 127) |
| 3 | `SERVER_HOST = "0.0.0.0"` força discovery UDP |
| 4 | Copiar cJSON `valuestring` para buffer local antes de `cJSON_Delete` |
| 5 | Retry de registro no `loop()`, não só no `setup()` |
| 6 | `WIFI_NONE_SLEEP` antes de `esp_now_init` (regra 21) |
| 7 | LED: `LED_ON LOW`, `LED_OFF HIGH` (GPIO2) |
| 8 | `device_name[32]` em todos os structs (regra 17) |
| 9 | Broadcast ESP8266→ESP32, unicast ESP32→ESP8266 ou ESP8266→ESP8266 (regra 18) |
| 10 | Validar strings EEPROM: caracteres 0x20–0x7E antes de usar (regra 20) |
| 11 | Dashboard "Detalhes" collapsível |
| 12 | Non-blocking loop — zero `delay()` |
| 13 | `FW_VERSION` = tag atual (shared_config.h) |
| 14 | `lib_extra_dirs` apontando para `../../shared` (nunca copiar shared) |
| 15 | `LORA_DEVICE` flag desativa ESP-NOW; `SENSOR_TYPE_ONOFF` define localmente |
| 16 | WiFi não-bloqueante: `mywifi_begin` + `mywifi_loop` + captive portal |
| 17 | Dashboard padrão: sidebar 180px, stats-header, polling 3s, footer-bar |
| 18 | API endpoints obrigatórios: `/api/state`, `/api/settings`, `/api/wifi`, `/api/ota`, `/api/restart` |
| 19 | Console/telnet obrigatório via `common_console.h` |
| 20 | EEPROM_SIZE = 512 (shared); relay state offset >=200 |
| 21 | Board pinos: respeitar `pins_arduino.h` (ex: TTGO LoRa32 V1 `LORA_RST=14`) |
| 22 | ESP32: `tzapu/WiFiManager @ ^2.0` (v0.x só ESP8266) |
| 23 | Se o dispositivo pode operar sem WiFi (ex: LoRa/ESP-NOW standalone), **adicionar seção "WiFi" no dashboard** com status (SSID/IP) e formulário para configurar rede — permite setup mesmo sem AP inicial ter acesso à internet |
| 24 | `POST /api/wifi` deve **salvar SSID+password na EEPROM** (offsets `EEPROM_WIFI_SSID_OFFSET`/`EEPROM_WIFI_PASS_OFFSET`) antes de chamar `WiFi.begin()`, senão no próximo boot o dispositivo não lembra da rede |
| 25 | Usar `console.printf` em vez de `Serial.printf` — `console` roteia para Serial + Telnet, essencial para debug remoto |
| 26 | Display OLED com páginas alternadas: página 0 (status principal: relé/pareamento/RSSI/IP), página 1 (estatísticas: TX/RX/Mem/Uptime), alternar a cada 5s sem `delay()` |
| 27 | Se dispositivo tem display OLED, **não duplicar info já visible em outras abas do dashboard** na seção WiFi — mostrar apenas "Rede Atual" (SSID + IP) e formulário de conexão |
| 28 | **`set_X()` dispara update**: toda função que altera estado do atuador (ex: `set_relay()`) deve chamar `lora_send_state()` / `espnow_send()` no final — assim botão, API, comando remoto e loop sempre enviam feedback, sem duplicar chamadas nos callers |
