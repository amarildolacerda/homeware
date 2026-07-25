# Spec: ESP32 PIR Sensor — Modo Bateria

## Visão Geral

Variante do cliente PIR para **ESP32 + LiPo** com foco em baixíssimo consumo. Opera em **deep sleep** >99% do tempo, acordando apenas quando o PIR dispara ou para heartbeat periódico. Não mantém WiFi ativo entre os wakes — cada wake é uma sessão curta (init → ESP-NOW → sleep).

A mesma base de código do `esp8266_pir`, com blocos `#ifdef BATTERY_MODE` para o comportamento alternativo.

## Arquitetura

```
Wake Trigger ──► ESP32 boot ──► setup()
  │                              │
  ├─ GPIO (PIR HIGH)             ├─ init WiFi + ESP-NOW (rápido)
  └─ Timer (RTC, ex: 5 min)     ├─ send broadcast (dado ou heartbeat)
                                  └─ esp_deep_sleep_start()
```

## Wake Sources

| Fonte | GPIO/Timer | Quando | Dado enviado |
|-------|-----------|--------|-------------|
| PIR trigger | GPIO RTC (ex: GPIO34) | Transição LOW→HIGH no PIR | `ESPNOW_MSG_SENSOR_DATA` com `motion_state=1` |
| Heartbeat timer | RTC timer | A cada `DEEP_SLEEP_TIMEOUT_S` segundos | `ESPNOW_MSG_HEARTBEAT` + bateria |

**Pós-wake PIR**: após enviar "presença detectada", o firmware pode opcionalmente fazer um `delay(10000)` para debounce e evitar múltiplos wakes consecutivos.

## Fluxo de Boot (`setup()`)

```
1. Inicializa RTC GPIO wake (PIR pin)
2. Configura timer wake (RTC)
3. Inicia WiFi em modo STA (sem portal)
4. ESP-NOW init + add peer broadcast
5. Identifica fonte do wake (PIR ou timer)
   - PIR → envia SENSOR_DATA (motion_state=1)
   - Timer → envia HEARTBEAT
6. Entra em deep sleep
```

Sem `loop()`, sem servidor web, sem OTA, sem telnet.

## Configuração Inicial (WiFiManager)

No **primeiro boot** (ou quando não há SSID salvo na NVS), o dispositivo permanece acordado por 5 minutos com o portal WiFiManager ativo (`ESP32_PIR_BAT` / `password123`). Após configurar WiFi, salva e reinicia em modo bateria.

Detecção de "primeiro boot": EEPROM/NVS com flag `config_done`. Se `0`, entra em modo config.

## Comunicação ESP-NOW

- **Sempre broadcast** (`FF:FF:FF:FF:FF:FF`), sem pareamento
- Gateway identifica o device pelo MAC source do frame
- Dado usa `espnow_header_t` com `sensor_type = SENSOR_TYPE_MOTION`
- Sem ACK wait — envia e dorme (fire-and-forget)
- Sem sequence number persistente entre sleeps (pode resetar)

## Payloads

### Sensor Data (PIR wake)
```
espnow_header_t {
  version, msg_type=ESPNOW_MSG_SENSOR_DATA,
  sequence=0, sensor_mac, sensor_type=SENSOR_TYPE_MOTION,
  battery_pct, rssi, payload_len
}
payload_motion_t { motion_state=1, occupancy_duration=0 }
```

### Heartbeat (timer wake)
```
espnow_header_t {
  version, msg_type=ESPNOW_MSG_HEARTBEAT,
  sequence=0, sensor_mac, sensor_type=SENSOR_TYPE_MOTION,
  battery_pct, rssi, payload_len=0
}
```

## Hardware

| Componente | ESP32 GPIO | Notas |
|------------|-----------|-------|
| PIR HC-SR501 | GPIO34 (RTC) | INPUT, wake por rising edge |
| LED (opcional) | GPIO2 | Active LOW |
| Bateria LiPo | VBAT → LDO 3.3V | 3.7V nominal |
| Sensor bateria | ADC1_CH0 (GPIO36) | Divisor opcional 2:1 |

### Pinos ESP32 recomendados (RTC GPIOs disponíveis para wake)

`GPIO34`, `GPIO35`, `GPIO36`, `GPIO39` são entradas-only sem resistor pull — ideais para PIR (já tem pull-down externo). Prefiro GPIO34.

## Consumo Estimado (ESP32)

| Estado | Corrente | Duração | Notas |
|--------|----------|---------|-------|
| Deep sleep (modem off) | ~10 µA | >99% do tempo | RTC timer + GPIO wake ativos |
| WiFi + ESP-NOW init | ~80 mA | ~30-50 ms | Inclui reconectar ao AP |
| Envio ESP-NOW | ~80 mA | ~5 ms | Broadcast |
| Pico no envio | ~180 mA | ~1 ms | RF transmit |
| **Médio (1 ev/min)** | **~15 µA** | | 5 envios de heartbeat/h + PIRs |
| **Médio (10 ev/min)** | **~100 µA** | | Uso intenso |

**Autonomia LiPo 500 mAh**: ~2-4 anos (15 µA médio). Perda por autodescarga domina.

## PlatformIO

```ini
[env:esp32_pir_bat]
board = esp32dev
platform = espressif32
build_flags = 
  -DBATTERY_MODE
  -DDEEP_SLEEP_WAKE_GPIO=34
  -DDEEP_SLEEP_TIMEOUT_S=300
  -DESPNOW_CHANNEL=${common.espnow_channel}
lib_extra_dirs = ${common.lib_extra_dirs}
```

## Variante Wired (inalterada)

O ESP8266 PIR existente (modo cabeado) continua com WiFi permanente, dashboard web, OTA e console. A `platformio.ini` terá dois targets:

| Environment | Plataforma | Modo | Uso |
|-------------|-----------|------|-----|
| `esp8266_pir` | ESP8266 | WIRED | Cabeado |
| `esp32_pir_bat` | ESP32 | BATTERY | Pilha/LiPo |

## Regras

1. `fire-and-forget`: sem ACK wait, sem retry — envia e dorme
2. Sem loop() em modo bateria — `setup()` faz tudo e chama `esp_deep_sleep_start()`
3. WiFi re-conecta a cada wake (tempo médio de conexão <50ms)
4. NVS salva SSID/password e flag `config_done` (primeiro boot)
5. Gateway distingue device pelo MAC (persistente entre sleeps)
6. ADC da bateria opcional (GPIO36), lido antes do sleep
7. LED pisca rápido no wake e apaga antes do sleep (feedback visual)
