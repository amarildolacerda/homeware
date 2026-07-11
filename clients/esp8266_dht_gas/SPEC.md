# Spec: ESP8266 DHT22 + MQ-2 Gas Sensor

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) sensor combinado de temperatura/umidade (DHT22) e gás (MQ-2). Comunica com o Gateway via ESP-NOW, envia dados periódicos de temperatura, umidade, nível de gás, alarme e status.

## Fluxo de Inicialização

1. **Hardware**: inicializa DHT22 (auto-detect GPIO), MQ-2 (A0 analógico, D6/GPIO16 digital), LEDs de alerta (GPIO12) e alarme (GPIO5), LED status (GPIO2)
2. **WiFi**: WiFiManager — se SSID salvo, tenta conectar; senão, abre portal `ESP8266_DHT_Gas` / `password123`
3. **ESP-NOW**: inicializa ESP-NOW em modo COMBO com canal fixo 1
4. **Gateway**: carrega MAC do gateway da EEPROM; se vazio, inicia pareamento
5. **Servidor web**: porta 80
6. **OTA**: ArduinoOTA pronto

## Componentes Funcionais

### Comunicação ESP-NOW

- **Pareamento**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast) a cada 10s; gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
- **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com `payload_dht_gas_t` (temperature, humidity, gas_level, alarm) + IP + heap livre
- **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` a cada 60s
- **ACK**: gateway confirma com `ESPNOW_MSG_ACK`; se negado, marca como não pareado
- **Sensor type**: `SENSOR_TYPE_DHT_GAS`

### Sensor DHT22

- **Pino padrão**: GPIO4 (D2), auto-detect em pinos candidatos: 4, 12, 13, 14, 0, 2, 15
- **Tipo**: DHT22
- **Fallback**: se leitura `isnan()`, mantém último valor válido com drift aleatório (temp ±0.5°C, hum ±2%), limitado a 18-30°C / 30-70%
- **Flag `dht_valid`**: `true` se leitura válida, `false` se fallback; incluído no `/api/state`

### Sensor MQ-2 (Gás)

- **Analógico (A0)**: leitura ADC mapeada para 0-100% (`map(raw, 0, 1024, 0, 100)`)
- **Digital (GPIO16/D6)**: leitura digital do pino digital do MQ-2
- **Thresholds**:
  - `GAS_ALERT_THRESHOLD = 15%` — LED alerta pisca lento (1s), alarme ativado
  - `GAS_ALARM_THRESHOLD = 30%` — LED alerta ligado fixo + LED alarme pisca rápido (200ms)
- **`s_alarm`**: `true` se gas_level >= alert threshold (15%)
- **Sensor error**: `true` se leitura analógica é 0 ou 1024 (possível desconectado/curto)

### LEDs de Status de Gás

| LED | Pino | Comportamento |
|-----|------|---------------|
| Alerta (GPIO12) | Gas normal | OFF |
| Alerta (GPIO12) | 15% <= gas < 30% | Pisca lento (1s) |
| Alerta (GPIO12) | gas >= 30% | Ligado fixo |
| Alarme (GPIO5) | gas < 30% | OFF |
| Alarme (GPIO5) | gas >= 30% | Pisca rápido (200ms) |

### Servidor Web (Dashboard)

- `GET /` — dashboard compacto: temperatura/umidade (badge), nível gás (badge), alarme; seção "Detalhes" collapsível (Gateway, RSSI, IP, Uptime, Bateria, DHT pin, LEDs, Versão)
- `GET /docs` — documentação da API
- `GET /api/state` — JSON completo: `temperature`, `humidity`, `gas_level`, `alarm`, `dht_valid`, `dht_pin`, `gas_analog_pin`, `gas_digital_pin`, `battery`, `device_id`, `device_name`, `fw_version`, `gateway_connected`, `paired`, `ip`, `rssi`, `uptime_s`, `last_send_s`, `slot`, `tx_count`, `rx_count`, `free_heap`, `raw_analog`, `gas_digital`, `led_state`, `alert_led_state`, `alarm_led_state`
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
| `t` | Testar pinos do DHT22 (auto-detect) |
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
- **LED status**: blink rápido (200ms) WiFi offline; blink lento (2s) não pareado; OFF quando pareado

## Regras Importantes

1. Toda mudança de estado dispara feedback imediato ao gateway via ESP-NOW
2. Device ID dinâmico (`esp8266_<chip_id>`), não configurável
3. Device name configurável via WiFiManager, salvo em EEPROM (> 32, < 127)
4. Dashboard usa layout compacto com "Detalhes" collapsível
5. Canal ESP-NOW fixo em 1
6. Estado do pareamento persiste em EEPROM (MAC do gateway)
7. DHT22 fallback: se leitura `isnan()`, mantém valor com drift aleatório (não envia dados inválidos)
8. `s_dht_valid` indica se a leitura DHT é confiável

## Hardware

| Componente | GPIO |
|------------|------|
| DHT22 | D2 (GPIO4) — auto-detect |
| MQ-2 analógico | A0 |
| MQ-2 digital | D6 (GPIO16) |
| LED alerta gás | D6 (GPIO12) |
| LED alarme gás | D5 (GPIO5) |
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
- `adafruit/DHT sensor library` — DHT22
