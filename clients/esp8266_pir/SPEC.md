# Spec: ESP8266 PIR Sensor (Presença)

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) sensor de presença PIR. Detecta movimento via sensor HC-SR501. Comunica com o Gateway via ESP-NOW, envia dados periódicos de presença, bateria e status.

## Fluxo de Inicialização

1. **Hardware**: inicializa pinos D2/GPIO4 (PIR), LED (GPIO2)
2. **WiFi**: WiFiManager — se SSID salvo, tenta conectar; senão, abre portal `ESP8266_PIR` / `password123`
3. **ESP-NOW**: inicializa ESP-NOW em modo COMBO com canal fixo 1
4. **Gateway**: carrega MAC do gateway da EEPROM; se vazio, inicia pareamento
5. **Servidor web**: porta 80
6. **OTA**: ArduinoOTA pronto

## Componentes Funcionais

### Comunicação ESP-NOW

- **Pareamento**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast) a cada 10s; gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
- **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com `payload_motion_t` (motion_state 0/1, occupancy_duration) + IP + heap livre
- **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` a cada 60s
- **ACK**: gateway confirma com `ESPNOW_MSG_ACK`; se negado (`PAIR_STATUS_DENIED`), marca como não pareado
- **Sensor type**: `SENSOR_TYPE_MOTION`

### Sensor PIR (HC-SR501)

- **Leitura digital (GPIO4/D2)**: HIGH = presença detectada, LOW = sem presença
- **Hold time**: após HIGH, mantém estado por `PIR_HOLD_TIME_MS` (30s) antes de voltar a LOW
- **Feedback imediato**: transição HIGH dispara envio ao gateway imediato (`s_last_espnow_send = 0`)
- **Bateria**: simulada, decrementa 1% a cada 100 leituras

### Servidor Web (Dashboard)

- `GET /` — dashboard compacto: estado de presença (badge); seção "Detalhes" collapsível (Gateway, RSSI, IP, Uptime, Versão)
- `GET /docs` — documentação da API
- `GET /api/state` — JSON completo: `motion_state`, `battery`, `device_id`, `device_name`, `fw_version`, `gateway_connected`, `paired`, `ip`, `rssi`, `uptime_s`, `slot`, `tx_count`, `rx_count`, `free_heap`
- `GET /api/settings` — configurações atuais (device_name)
- `POST /api/settings` — altera nome com `{"device_name":"..."}`
- `GET /api/pin?gpio=N` — leitura digital de GPIO
- `POST /api/pin` — escrita digital com `{"gpio":N,"state":0|1}`
- `POST /api/ota` — upload de firmware (multipart)

### Terminal Serial (115200 baud)

| Tecla | Ação |
|-------|------|
| `l` | Leitura forçada do PIR + envio ao gateway |
| `s` | Status do dispositivo |
| `p` | Resetar par e tentar parear |
| `u` | Info OTA |
| `r` | Restart |
| `h` / `?` | Help |

Suporte a telnet na porta 23 com os mesmos comandos.

## Loop (Non-blocking State Machine)

Nenhum `delay()` bloqueante no `loop()` (exceto WiFi reconnect e ack wait). Usa máquina de estados com timestamps:

- **Sensor**: leitura contínua do PIR a cada iteração (via `read_pir()`)
- **Envio**: envia dados a cada `STATE_UPDATE_INTERVAL` (5s) ou imediatamente na transição HIGH/LOW
- **ACK wait**: `ESPNOW_ACK_TIMEOUT_MS` (200ms) + retries (`ESPNOW_SEND_RETRIES` = 3)
- **Pareamento**: retry a cada 10s, máx 30 tentativas, pausa de 60s após max
- **Heartbeat**: a cada 60s
- **LED**: blink rápido (200ms) WiFi offline; blink lento (2s) não pareado; OFF quando pareado

## Regras Importantes

1. Toda mudança de estado dispara feedback imediato ao gateway via ESP-NOW
2. Device ID dinâmico (`esp8266_<chip_id>`), não configurável
3. Device name configurável via WiFiManager, salvo em EEPROM (> 32, < 127)
4. Dashboard usa layout compacto com "Detalhes" collapsível
5. Canal ESP-NOW fixo em 1
6. Estado do pareamento persiste em EEPROM (MAC do gateway)

## Hardware

| Componente | GPIO |
|------------|------|
| PIR HC-SR501 | D2 (GPIO4) |
| LED (built-in) | D4 (GPIO2) — Active LOW |

## LED Status

| Estado | Comportamento |
|--------|--------------|
| WiFi Config Portal | Solid ON |
| WiFi offline | Fast blink (200ms) |
| WiFi ok, não pareado | Slow blink (2s) |
| Pareado + conectado | OFF |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
