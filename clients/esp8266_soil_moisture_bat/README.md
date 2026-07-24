# ESP8266 Soil Moisture Sensor (ESP-NOW)

Cliente ESP8266 para sensor capacitivo de humidade do solo.
Comunica com o Gateway via ESP-NOW. Bateria + deep sleep.

## Configuração

Segurar D3 (GND) ao ligar → portal WiFiManager `ESP8266_SoilMoisture`.
Configure WiFi, nome e intervalo de leitura.

## Hardware

| Componente | GPIO |
|------------|------|
| Sensor (analógico) | A0 |
| LED | D4/GPIO2 |
| Botão config | D3/GPIO0 (GND no boot) |
| Wake | D0/GPIO16 → RST |

## Build

```bash
./build.sh
./flash.sh /dev/ttyUSB0
```

## API

Sem web server em runtime (deep sleep). Configuração via WiFiManager apenas.
