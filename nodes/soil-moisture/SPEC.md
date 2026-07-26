# Spec: ESP8266 Soil Moisture Sensor

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini) sensor capacitivo de humidade do solo.
Comunica com o Gateway via ESP-NOW. Alimentado por bateria com deep sleep.

## Fluxo de Inicialização

1. **Hardware**: init ADC (A0), LED (GPIO2), botão config (GPIO0)
2. **WiFi**: se botão pressionado ou sem gateway MAC → portal WiFiManager (2 min)
   - Configura: SSID, senha, device name, intervalo (min)
3. **Pareamento**: após portal, envia PAIR_REQUEST a cada 3s (máx 30s)
4. **Normal**: WiFi on → ESP-NOW → lê ADC → envia dados → deep sleep

## Regras

1. Só envia dados se gateway MAC conhecido (salvo em EEPROM)
2. Broadcast ESP8266→ESP32 (regra 18)
3. `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes de `esp_now_init` (regra 21)
4. Deep sleep `ESP.deepSleep()`, sem delay bloqueante

## Hardware

| Componente | GPIO |
|------------|------|
| Sensor capacitivo | A0 |
| LED | D4/GPIO2 |
| Botão config | D3/GPIO0 (GND = configura) |
| Wake | D0/GPIO16 → RST |

## Dependências

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16
