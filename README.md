# AgriSense IoT

Sistema de automação inteligente com sensores/atuadores ESP-NOW, hub gateway e server Python integrado ao Home Assistant.

```
Sensores/Atuadores ──► Hub (ESP-NOW) ──► Server (Python) ──► Home Assistant
       nodes              gateway            HTTP REST         MQTT Discovery
```

## O que é

AgriSense IoT permite monitorar e controlar dispositivos físicos (relés, sensores de temperatura, umidade, gás, presença, chuva) de forma distribuída via protocolo ESP-NOW, com integração nativa ao Home Assistant via MQTT Discovery.

## Componentes

- **Hub** — gateway entre ESP-NOW e rede; recebe dados dos nodes e encaminha ao server; dashboard web com OTA, pareamento e logs
- **Server** — servidor Python (add-on Home Assistant); discovery UDP, API REST, MQTT Discovery
- **Nodes** — firmwares PlatformIO (ESP8266/ESP32) para sensores e atuadores; comunicação exclusiva via ESP-NOW com o hub

## Nodes

| Node | Função | Estado |
|------|--------|--------|
| `lamp` | Relé ON/OFF + Alexa + extender ESP-NOW | **estável** v1.2.8 |
| `switch` | Relé ON/OFF + pulse/timer + LoRa | **estável** v1.2.8 |
| `climate-gas` | DHT22 + MQ-2 (temp/umidade/gás) | desenvolvimento |
| `presence` | Sensor de presença PIR | desenvolvimento |
| `rain` | Sensor de chuva | desenvolvimento |
| `extender` | Extensor de alcance ESP-NOW | desenvolvimento |

## Funcionalidades

- Dashboard web responsivo (ciclo, timer, sincronização entre devices)
- OTA via browser
- Pareamento manual via dashboard ou serial
- Re-registro automático via broadcast UDP
- Integração com Home Assistant (MQTT Discovery)
- Suporte a Alexa (Espalexa) nos relés

## Quick Start

```bash
# Build
platformio run -d . -e lamp_ota

# Flash (serial)
./flash.sh -p /dev/ttyUSB0

# Monitor serial
./monitor.py

# Descobrir devices na LAN
python scan.py
```

## Repositórios

| Módulo | Repo |
|--------|------|
| `server/` | [homeware_bridge](https://github.com/amarildolacerda/homeware_bridge) |
| `shared/` | [homeware_shared](https://github.com/amarildolacerda/homeware_shared) |

## Documentação

- [TECHNICAL.md](TECHNICAL.md) — arquitetura detalhada, regras de design, estrutura de diretórios e dependências
- [AGENTS.md](AGENTS.md) — regras de branch, build, submodule e contribuição
