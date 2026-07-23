# Homeware

Sistema de automação residencial com bridge Python (Home Assistant), gateway ESP-NOW e clients ESP8266 para sensores/atuadores.

```
Sensores/Atuadores ──► Gateway (ESP-NOW ch.1) ──► Bridge (HTTP) ──► Home Assistant (MQTT)
  (ESP8266)           (ESP8266/ESP32)            (Python)            (MQTT Discovery)
```

## Repositórios

| Submódulo | Repo |
|-----------|------|
| `bridge/` | [homeware_bridge](https://github.com/amarildolacerda/homeware_bridge) |
| `shared/` | [homeware_shared](https://github.com/amarildolacerda/homeware_shared) |

## Gateway (`gateway/`)

Recebe dados dos clients via ESP-NOW (canal 1), encaminha ao bridge via HTTP REST.
- Dashboard web (OTA, pareamento, logs, status)
- Registro de sensores com persistência NVS
- MQTT Discovery automático (entidades individuais)
- Suporte ESP8266, ESP32, ESP32C3
- Comando via serial: `l` (list), `s` (status), `d <id>` (detalhes), `b` (broadcast re-register)

## Clients (`clients/`)

Firmwares PlatformIO para ESP8266 que se comunicam exclusivamente via ESP-NOW com o gateway (sem HTTP direto).

| Client | Descrição | Estado |
|--------|-----------|--------|
| `esp8266_lampada` | Relé ON/OFF + Alexa (Espalexa) + repeater | **estável** v0.0.30 |
| `esp8266_onoff` | Relé ON/OFF simples | **estável** v0.0.30 |
| `esp8266_dht_gas` | DHT22 + MQ-2 (temp/umidade/gás) | **estável** v0.0.30 |
| `esp8266_pir` | Sensor de presença PIR | **estável** v0.0.30 |
| `esp8266_chuva` | Sensor de chuva | desenvolvimento |
| `esp8266_repeater` | Extensor de alcance ESP-NOW | desenvolvimento |

### Funcionalidades comuns
- Dashboard web responsivo (ciclo, timer, pulso, sincronização entre devices)
- OTA via browser
- Pareamento manual via dashboard ou serial
- Re-registro automático via broadcast UDP
- LED com lógica invertida (GPIO2, LOW = ligado)

## Quick Start (desenvolvimento)

```bash
# Build todos os firmwares
platformio run -d . -e esp8266_lamp_ota

# Flash via serial
./flash.sh -p /dev/ttyUSB0

# Monitor
./monitor.py

# Scan devices na LAN
python scan.py
```

## Dependências

- [PlatformIO](https://platformio.com/) (`pip install platformio`)
- Python 3.10+

## Regras de Design

1. **ESP-NOW**: client ESP8266 → gateway ESP32 usa **broadcast** (unicast falha c/ AP ativo); ESP32 → ESP8266 ou ESP8266 → ESP8266 usa unicast
2. **WiFi**: `WIFI_NONE_SLEEP` obrigatório em clients ESP-NOW para não perder pacotes
3. **Dashboard**: páginas web enxutas (<8KB) ou chunked via `WiFiClient` para não estourar heap
4. **Loop non-blocking**: sem `delay()`, máquina de estados com `millis()`
5. **Device ID**: dinâmico `esp8266_<chip_id>`, nome configurável via WiFiManager
6. **MQTT**: entity_id sem ponto (usa `_`), slot é o último segmento após `_`

## Device Types Suportados

`onoff`, `temperature`, `humidity`, `gas`, `rain`, `contact`, `occupancy`, `light_sensor`, `dimmable`, `dht_gas`

## Desenvolvimento

Ver [AGENTS.md](AGENTS.md) para regras de branch, build, submodule e contribuição.
