# Spec: ESP8266 Lâmpada (ON/OFF Relay)

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) que controla um relé ON/OFF. Comunica com o Gateway via ESP-NOW, suporta Alexa (Fauxmo/UPnP) e atua como repeater ESP-NOW.

## Fluxo de Inicialização

1. **Hardware**: inicializa relé (GPIO4), LED (GPIO2), carrega estado do relé da EEPROM
2. **WiFi**: WiFiManager — se SSID salvo, tenta conectar; senão, abre portal
3. **ESP-NOW**: inicializa ESP-NOW em modo COMBO
4. **Gateway**: carrega MAC do gateway da EEPROM; se vazio, inicia pareamento
5. **Servidor web**: inicia na porta DASHBOARD_PORT (80)
6. **Alexa**: inicia Fauxmo na porta FAUXMO_PORT (8080)
7. **OTA**: ArduinoOTA pronto

## Componentes Funcionais

### Comunicação ESP-NOW

- **Pareamento**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast) em intervalo definido por `ESPNOW_PAIR_INTERVAL_MS`; gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
- **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com `payload_onoff_t` (state 0/1) + IP embutido
- **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` a cada `HEARTBEAT_INTERVAL`
- **Comando**: recebe `ESPNOW_MSG_COMMAND` do gateway; checa `target_mac` antes de executar
- **ACK**: gateway confirma recebimento com `ESPNOW_MSG_ACK`; se negado (`PAIR_STATUS_DENIED`), marca como não pareado
- **Feedback imediato**: toda mudança de estado no relé (`set_relay()`) zera `s_last_espnow_send` para enviar ao gateway no próximo `loop()`

### Repeater (Range Extender)

- Todo pacote recebido via ESP-NOW que não seja para o próprio dispositivo é reenviado:
  - Do gateway → broadcast (outros clients checam `target_mac`)
  - De outro client → gateway
- Ativado automaticamente quando pareado; não requer configuração extra

### Repeater MAC (Modo Forçado)

- Se `REPEATER_MAC` está definido (config.h ou WiFiManager), o cliente pula pareamento e envia todos os pacotes para o MAC do repeater
- O repeater encaminha ao gateway

### Alexa (Fauxmo/UPnP)

- Emula dispositivo Belkin Wemo
- Nome do dispositivo: configurado via WiFiManager (padrão: `Luz Lampada`)
- Callback `fauxmo_callback` dispara `set_relay()` e envia feedback imediato ao gateway via ESP-NOW

### Servidor Web (Dashboard)

- `GET /` — dashboard compacto com botão ON/OFF + badge estado; seção "Detalhes" collapsível (Gateway, RSSI, IP, Uptime, Versão)
- `GET /api/state` — JSON completo do dispositivo
- `GET /api/relay` — estado do relé
- `POST /api/relay` — com `{"state":true/false}` controla relé
- `GET /api/pin` — estado digital de um GPIO
- `POST /api/pin` — com `{"gpio":N,"state":0|1}` controla GPIO
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

## Regras Importantes

1. Qualquer mudança de estado (dashboard, Alexa, serial) deve disparar feedback imediato ao gateway via ESP-NOW
2. Ao receber COMMAND do gateway, verificar `target_mac` antes de executar (evita execução duplicada em broadcast)
3. Estado do relé persiste em EEPROM entre reboots
4. Device ID é dinâmico (`esp8266_<chip_id>`), não configurável
5. Device name configurável via WiFiManager, salvo em EEPROM com validação (> 32, < 127)
6. Dashboard usa layout compacto com seção "Detalhes" collapsível

## LED Status

| Estado | Comportamento |
|--------|--------------|
| WiFi Config Portal | Solid ON |
| WiFi offline | Fast blink (250ms) |
| Não pareado | Slow blink (500ms) |
| Pareado + conectado | OFF |

## Hardware

| Componente | GPIO |
|------------|------|
| Relé | D2 (GPIO4) — Active HIGH |
| LED (built-in) | D4 (GPIO2) — Active LOW |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16
- `xoseperez/fauxmoesp` — emulação Alexa
- `arduino-libraries/ESP8266mDNS`
