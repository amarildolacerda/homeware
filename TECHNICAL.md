# AgriSense IoT — Documentação Técnica

## Arquitetura

```
Sensores/Atuadores ──► Hub (ESP-NOW ch.1) ──► Server (HTTP) ──► Home Assistant (MQTT)
  (AgriSense Nodes)          (Hub)                 (Python)            (MQTT Discovery)
```

- **Hub**: recebe dados dos nodes via ESP-NOW, encaminha ao server via HTTP REST; gateway entre ESP-NOW e WiFi/LAN
- **Server**: servidor HTTP REST + HA + discovery UDP + MQTT Discovery
- **Nodes**: firmwares ESP8266/ESP32 que se comunicam com o hub via ESP-NOW (sem HTTP direto)

## Repositórios (Submódulos)

| Submódulo | Repo |
|-----------|------|
| `server/` | [homeware_bridge](https://github.com/amarildolacerda/homeware_bridge) |
| `shared/` | [homeware_shared](https://github.com/amarildolacerda/homeware_shared) |

## Hub (`hub/`)

Recebe dados dos nodes via ESP-NOW (canal 1), encaminha ao server via HTTP REST.

- Dashboard web (OTA, pareamento, logs, status)
- Registro de sensores com persistência NVS
- MQTT Discovery automático (entidades individuais)
- Suporte ESP8266, ESP32, ESP32C3
- Comando via serial: `l` (list), `s` (status), `d <id>` (detalhes), `b` (broadcast re-register)

## Nodes (`nodes/`)

Firmwares PlatformIO para ESP8266/ESP32 que se comunicam exclusivamente via ESP-NOW com o hub.

| Node | Descrição | Estado |
|------|-----------|--------|
| `lamp` | Relé ON/OFF + Alexa (Espalexa) + extender | **estável** v1.2.8 |
| `switch` | Relé ON/OFF simples + pulse/timer + LoRa | **estável** v1.2.8 |
| `climate-gas` | DHT22 + MQ-2 (temp/umidade/gás) | desenvolvimento |
| `presence` | Sensor de presença PIR | desenvolvimento |
| `rain` | Sensor de chuva | desenvolvimento |
| `extender` | Extensor de alcance ESP-NOW | desenvolvimento |

### Funcionalidades comuns dos nodes

- Dashboard web responsivo (ciclo, timer, pulso, sincronização entre devices)
- OTA via browser
- Pareamento manual via dashboard ou serial
- Re-registro automático via broadcast UDP
- LED com lógica invertida (GPIO2, LOW = ligado)

## Regras de Design

1. **ESP-NOW**: node ESP8266 → hub ESP32 usa **broadcast** (unicast falha c/ AP ativo); ESP32 → ESP8266 ou ESP8266 → ESP8266 usa unicast
2. **WiFi**: `WIFI_NONE_SLEEP` obrigatório em nodes ESP-NOW para não perder pacotes
3. **Dashboard**: páginas web enxutas (<8KB) ou chunked via `WiFiClient` para não estourar heap
4. **Loop non-blocking**: sem `delay()`, máquina de estados com `millis()`
5. **Device ID**: dinâmico `agri_<chip_id>`, nome configurável via WiFiManager
6. **MQTT**: entity_id sem ponto (usa `_`), slot é o último segmento após `_`
7. **MQTT Discovery**: topico sem `.` no entity_id; payload JSON aceita `.`, mas manter `_` por consistência
8. **Device name**: 32 bytes (`espnow_pair_request_t.device_name`), salvo em EEPROM com validação

## Device Types Suportados

`onoff`, `temperature`, `humidity`, `gas`, `rain`, `contact`, `occupancy`, `light_sensor`, `dimmable`, `dht_gas`

## Quick Start (desenvolvimento)

```bash
# Build todos os firmwares
platformio run -d . -e lamp_ota

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
- Git submodules (`git submodule update --init --recursive`)

## Estrutura de Diretórios

```
agrisense-iot/
├── server/          # Servidor Python (Home Assistant add-on)
├── shared/          # Código cross-platform (submodule)
├── hub/             # Hub ESP-NOW (ESP8266/ESP32)
├── nodes/
│   ├── lamp/        # Relé ON/OFF + Alexa
│   ├── switch/      # Relé ON/OFF + pulse/timer
│   ├── climate-gas/ # DHT22 + MQ-2
│   ├── presence/    # Sensor PIR
│   ├── rain/        # Sensor de chuva
│   └── extender/    # Extensor ESP-NOW
├── tests/           # Testes unitários
├── scripts/         # Scripts auxiliares
└── wifimanager/     # Strings WiFiManager
```
