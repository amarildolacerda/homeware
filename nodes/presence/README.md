# ESP8266 PIR Sensor Client (ESP-NOW)

Cliente ESP8266 para sensor de presença PIR (HC-SR501). Comunica com o **ESP-NOW Gateway** via ESP-NOW, que encaminha os dados ao bridge via HTTP.

## Funcionalidades

- Detecção de presença via PIR HC-SR501
- Hold time configurável (30s padrão) para evitar transições rápidas
- LED de status (WiFi, gateway, config portal)
- Comandos via terminal serial / telnet (pressione 'h' para ajuda)
- Pareamento automático com gateway ESP-NOW
- Envio de dados via ESP-NOW
- Heartbeat periódico via ESP-NOW
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)
- OTA via ArduinoOTA e upload HTTP (`/api/ota`)

## Hardware

| Componente | GPIO | Notas |
|------------|------|-------|
| PIR HC-SR501 | D2/GPIO4 | Saída digital HIGH = presença detectada |
| LED built-in | D4/GPIO2 | Active LOW |

## Wiring

| PIR Pin | ESP8266 Pin |
|---------|-------------|
| VCC | 5V ou 3.3V |
| GND | GND |
| OUT | D2/GPIO4 |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "PIR ESP-NOW"
#define PIR_HOLD_TIME_MS 30000   // tempo de retenção do estado HIGH
#define STATE_UPDATE_INTERVAL 5000
```

## Build (PlatformIO)

```bash
./build.sh
./flash.sh
```

Ou manualmente:

```bash
pio run -t upload
pio device monitor
```

## OTA

Via ArduinoOTA (hostname: `{device_id}.local`, porta 8266):

```bash
pio run -e esp8266_ota -t upload
```

Via upload HTTP:

```bash
curl -F "firmware=@.pio/build/esp8266/firmware.bin" http://esp8266_xxxxxx.local/api/ota
```

## Terminal / Atalhos (115200 baud)

| Tecla | Ação |
|-------|------|
| `l` | Leitura forçada do PIR + envio ao gateway |
| `s` | Status do dispositivo |
| `p` | Resetar par e tentar parear |
| `u` | Info OTA |
| `r` | Restart |
| `h`/`?` | Ajuda |

Terminal via telnet na porta 23.

## API local (no ESP8266)

| Endpoint | Método | Descrição |
|----------|--------|-----------|
| `/` | GET | Dashboard |
| `/docs` | GET | Documentação da API |
| `/api/state` | GET | JSON estado completo |
| `/api/settings` | GET/POST | Configurações (device_name) |
| `/api/pin?gpio=N` | GET | Leitura digital de GPIO |
| `/api/pin` | POST | Escrita digital: `{"gpio":N,"state":0\|1}` |
| `/api/ota` | POST | Upload firmware (multipart) |

## ESP-NOW Protocol

O cliente implementa o protocolo `espnow_protocol.h`:

1. **Pairing**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast), gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
2. **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com payload `payload_motion_t` (motion_state, occupancy_duration) + IP + heap livre
3. **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` periodicamente
4. **ACK**: gateway responde com `ESPNOW_MSG_ACK`

MAC do gateway é salvo em EEPROM para persistência entre reboots.

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
