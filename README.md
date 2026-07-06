# Homeware

Smart home system com bridge Python, gateway ESP-NOW e clients ESP8266 para sensores/atuadores.

## Arquitetura

```
Sensores ESP-NOW ──► Gateway (ESP8266) ──► Bridge (Python) ──► Home Assistant
  Atuadores       ◄──                        ◄── MQTT Commands
```

### bridge/
Bridge Python que recebe dados de dispositivos via HTTP e publica entidades no Home Assistant via MQTT Discovery. Painel web embutido.

### gateway/
Gateway ESP-NOW que recebe dados dos clients (canal 1) e encaminha ao bridge via HTTP REST. Dashboard web, pareamento por botão, OTA.

### clients/
Firmwares Arduino para sensores/atuadores ESP8266 que se registram no gateway via ESP-NOW:
- `esp8266_chuva/` — sensor de chuva
- `esp8266_dht_gas/` — sensor de temperatura, umidade + gás
- `esp8266_lampada/` — relé ON/OFF com Alexa (Fauxmo) + função repeater
- `esp8266_repeater/` — extensor de alcance ESP-NOW

## Quick Start

```bash
docker compose up -d
```

Sobe Mosquitto, Home Assistant e o bridge. Acesse `http://<ip>:80/` para o painel do bridge.

## Device Types Suportados

onoff, temperature, humidity, gas, rain, contact, occupancy, light_sensor, tanque, dimmable, electricity, dht_gas

## Desenvolvimento

Ver [AGENTS.md](AGENTS.md) para regras de branch, build e contribuição.
