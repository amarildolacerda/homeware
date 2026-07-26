# Spec: ESP8266 Rain Sensor (Chuva)

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) sensor de chuva. Monitora nível de chuva via sensor analógico e digital. Comunica com o Gateway via ESP-NOW, envia dados periódicos de chuva, bateria e status.

## Fluxo de Inicialização

1. **Hardware**: inicializa pinos A0 (analógico), D6/GPIO16 (digital), LED (GPIO2)
2. **WiFi**: WiFiManager — se SSID salvo, tenta conectar; senão, abre portal `ESP8266_Chuva` / `password123`
3. **ESP-NOW**: inicializa ESP-NOW em modo COMBO com canal fixo 1
4. **Gateway**: carrega MAC do gateway da EEPROM; se vazio, inicia pareamento
5. **Servidor web**: porta 80
6. **OTA**: ArduinoOTA pronto

## Componentes Funcionais

### Comunicação ESP-NOW

- **Pareamento**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast) a cada 10s; gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
- **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com `payload_rain_t` (rain_level 0-100%, rain_digital) + IP + heap livre
- **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` a cada 60s
- **ACK**: gateway confirma com `ESPNOW_MSG_ACK`; se negado (`PAIR_STATUS_DENIED`), marca como não pareado
- **Sensor type**: `SENSOR_TYPE_RAIN`

### Sensor de Chuva

- **Analógico (A0)**: leitura ADC mapeada para 0-100% (0 = muita chuva, 100 = seco). `map(raw, 0, 1024, 100, 0)` + `constrain(0, 100)`
- **Digital (GPIO16/D6)**: leitura digital. LOW = chuva, HIGH = seco. No payload, `rain_digital` é invertido (true = chuva)
- **Bateria**: simulada, decrementa 1% a cada 100 leituras (~8 min)

### Servidor Web (Dashboard)

- `GET /` — dashboard compacto: nível de chuva (badge), estado digital (chuva/seco); seção "Detalhes" collapsível (Gateway, RSSI, IP, Uptime, Bateria, Versão)
- `GET /docs` — documentação da API
- `GET /api/state` — JSON completo: `rain_level`, `rain_digital`, `battery`, `device_id`, `device_name`, `fw_version`, `gateway_connected`, `paired`, `ip`, `rssi`, `uptime_s`, `slot`, `tx_count`, `rx_count`, `free_heap`
- `GET /api/settings` — configurações atuais (device_name)
- `POST /api/settings` — altera nome com `{"device_name":"..."}`
- `GET /api/pin?gpio=N` — leitura digital de GPIO
- `POST /api/pin` — escrita digital com `{"gpio":N,"state":0|1}`
- `POST /api/ota` — upload de firmware (multipart)

### Terminal Serial (115200 baud)

| Tecla | Ação |
|-------|------|
| `l` | Leitura forçada + envio ao gateway |
| `s` | Status do dispositivo |
| `p` | Resetar par e tentar parear |
| `u` | Info OTA |
| `h` / `?` | Help |
| `r` | Restart |

## Loop (Non-blocking State Machine)

Nenhum `delay()` bloqueante no `loop()` (exceto WiFi reconnect e ack wait). Usa máquina de estados com timestamps:

- **Sensor**: leitura a cada `STATE_UPDATE_INTERVAL` (5s)
- **Envio**: envia dados a cada `STATE_UPDATE_INTERVAL` (5s)
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
| Sensor chuva analógico | A0 |
| Sensor chuva digital | D6 (GPIO16) |
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
- `tzapu/WiFiManager` ^2.0.16
