# Spec: ESP8266 Lâmpada (ON/OFF Relay)

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) que controla um relé ON/OFF. Comunica com o Gateway via ESP-NOW, suporta Alexa (Espalexa/Hue Bridge) e atua como repeater ESP-NOW bidirecional.

## Fluxo de Inicialização

1. **Hardware**: inicializa relé (GPIO4), LED (GPIO2), carrega estado do relé da EEPROM
2. **WiFi**: WiFiManager — se SSID salvo, tenta conectar; senão, abre portal. Modo `WIFI_AP_STA` com `ESP_NOW_CHANNEL 1` fixo. Se WiFi falha, não reinicia — ESP-NOW continua operando sem roteador
3. **ESP-NOW**: inicializa ESP-NOW em modo COMBO com canal fixo 1
4. **Gateway**: carrega MAC do gateway da EEPROM; se vazio, inicia pareamento
5. **Servidor web**: Espalexa gerencia o server na porta 80
6. **Alexa**: Espalexa (Hue Bridge SSDP + UPnP), porta 80
7. **OTA**: ArduinoOTA pronto

## Componentes Funcionais

### Comunicação ESP-NOW

- **Pareamento**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast) a cada `ESPNOW_PAIR_INTERVAL_MS`; gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
- **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com `payload_onoff_t` (state 0/1) + IP embutido
- **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` a cada `HEARTBEAT_INTERVAL`
- **Comando**: recebe `ESPNOW_MSG_COMMAND` do gateway; checa `target_mac` antes de executar
- **ACK**: gateway confirma com `ESPNOW_MSG_ACK`; se negado (`PAIR_STATUS_DENIED`), marca como não pareado
- **Feedback imediato**: `set_relay()` zera `s_last_espnow_send` para enviar ao gateway no próximo loop
- **WiFi sem roteador**: `WIFI_AP_STA` com `ESP_NOW_CHANNEL 1`; não reinicia se WiFi falha

### Repeater (Range Extender) Bidirecional

- Todo pacote recebido via ESP-NOW que não seja para o próprio dispositivo é reenviado:
  - Do gateway → broadcast (outros clients checam `target_mac`)
  - De outro client → gateway
- Bidirecional: encaminha do gateway para clients E de clients para o gateway
- Ativado via WiFiManager (padrão: desligado)
- Configurado em `config.h` com `#define REPEATER_DEFAULT false`

### Repeater MAC (Modo Forçado)

- Se `REPEATER_MAC` está definido, client pula pareamento e envia todos os pacotes para o MAC do repeater

### Alexa (Espalexa/Hue Bridge)

- Emula lâmpada Hue Bridge via SSDP + UPnP na porta 80
- Nome: configurado via WiFiManager (padrão: `Luz Lampada`)
- Callback `alexa_callback(EspalexaDevice*)` dispara `set_relay()` + feedback imediato via ESP-NOW
- Status `alexa_connected` no `/api/state` baseado em último comando (janela de 10 min)

### Servidor Web (Dashboard)

- `GET /` — dashboard compacto: botão ON/OFF + badge estado; seção "Detalhes" collapsível (Gateway, RSSI, IP, Uptime, Alexa, Versão)
- `GET /docs` — documentação da API
- `GET /api/state` — JSON completo do dispositivo (state, button, battery, device_id, device_name, gateway_connected, paired, ip, rssi, uptime_s, slot, alexa_connected, fw_version, last_send_s)
- `GET /api/relay` — estado do relé `{"state":bool}`
- `POST /api/relay` — controla relé com `{"state":true/false}`
- `GET /api/pin?gpio=N` — leitura digital de GPIO
- `POST /api/pin` — escrita digital com `{"gpio":N,"state":0|1}`
- `GET /api/settings` — configurações atuais
- `POST /api/settings` — altera nome/pinos com `{"device_name":"...","relay_pin":N,"button_pin":N}`
- `POST /api/restart` — reinicia o dispositivo
- `POST /api/ota` — upload de firmware (multipart)

### Terminal Serial (115200 baud)

| Tecla | Ação |
|-------|------|
| `l` | Toggle relay |
| `0` | Turn off |
| `1` | Turn on |
| `s` | Device status |
| `p` | Reset pairing |
| `u` | OTA info |
| `a` | Alexa device info |
| `h` / `?` | Help |
| `r` | Restart |

## Loop (Non-blocking State Machine)

Nenhum `delay()` bloqueante no `loop()`. Todos os ciclos usam máquina de estados com timestamps:

- **Pareamento**: `s_last_espnow_pair` + `s_pair_wait_until` para backoff de 60s após falhas
- **Envio**: `s_send_pending` + `s_send_deadline` + `s_send_retries_left` — ACK wait e retries sem bloquear
- **LED**: timestamps por estado (WiFi blink 250ms, unparied blink 500ms)
- **WiFi**: `maintain_wifi_connection()` sem `return` precoce, apenas verifica a cada loop
- **Alexa**: `s_alexa.loop()` a cada iteração

## Regras Importantes

1. Toda mudança de estado (dashboard, Alexa, serial) dispara feedback imediato ao gateway via ESP-NOW (zera `s_last_espnow_send`)
2. Ao receber COMMAND do gateway, verificar `target_mac` antes de executar (evita duplicação em broadcast)
3. Estado do relé persiste em EEPROM entre reboots
4. Device ID dinâmico (`esp8266_<chip_id>`), não configurável
5. Device name configurável via WiFiManager, salvo em EEPROM (> 32, < 127)
6. Dashboard usa layout compacto com "Detalhes" collapsível
7. Nenhum `delay()` bloqueante no loop principal
8. Canal ESP-NOW fixo em 1 (`ESP_NOW_CHANNEL`), mesmo sem roteador

## LED Status

| Estado | Comportamento |
|--------|--------------|
| WiFi Config Portal | Solid ON |
| WiFi offline | Fast blink (250ms) |
| WiFi ok, não pareado | Slow blink (500ms) |
| Pareado + conectado | OFF |

## Hardware

| Componente | GPIO |
|------------|------|
| Relé | D2 (GPIO4) — Active HIGH |
| LED (built-in) | D4 (GPIO2) — Active LOW |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16
- `Aircoookie/Espalexa` ^2.7.0 — emulação Alexa (Hue Bridge)
- `arduino-libraries/ESP8266mDNS`
