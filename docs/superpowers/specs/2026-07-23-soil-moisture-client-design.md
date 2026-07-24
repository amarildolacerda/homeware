# Sensor de Humidade do Solo (ESP-NOW, Bateria, Deep Sleep)

## Visão Geral

Cliente ESP8266 (Wemos D1 Mini)/ESP32 S3 Mini com sensor capacitivo de humidade do solo.
Comunica com o Gateway via ESP-NOW. Alimentado por bateria, usa deep sleep
entre leituras. Sem web server em runtime — configuração via WiFiManager
no boot (portal de 2 min).

## Ciclo de Vida

```
Boot (power-on ou timer deep sleep)
  │
  ├─ BOOT CONFIG (botão D3/GND pressionado OU EEPROM vazia):
  │     → Portal WiFiManager por 2 min
  │         Configura: SSID, senha, device name, intervalo (1-60 min)
  │     → Salva em EEPROM
  │     → Deep sleep pelo intervalo configurado
  │     → FIM
  │
  └─ BOOT NORMAL:
        → WiFi on (STA, conecta ao SSID salvo, timeout 5s)
        → init ESP-NOW (COMBO, canal 1, WIFI_NONE_SLEEP)
        → Lê ADC do sensor (0-1024)
        → Mapeia raw → moisture_pct (0-100%) com constrain
        → Envia ESP-NOW broadcast: SENSOR_DATA (payload_soil_moisture_t)
        → LED pisca rápido (feedback visível)
        → WiFi off
        → Deep sleep pelo intervalo configurado
```

## Componentes Funcionais

### Sensor Capacitivo

- **Leitura analógica (A0)**: ADC 0-1024 raw
- **Payload**: `raw_adc` (uint16_t, valor bruto) + `moisture_pct` (uint8_t, 0-100%)
- **Fórmula**: `moisture_pct = constrain(map(raw, 0, 1024, 0, 100), 0, 100)`
- Bateria enviada no campo `battery_pct` do header ESP-NOW
- Bateria simulada: decremento percentual por número de ciclos

### Comunicação ESP-NOW

- **Envio**: `ESPNOW_MSG_SENSOR_DATA` via **broadcast** (regra 18)
- **Sensor type**: `SENSOR_TYPE_SOIL_MOISTURE = 12`
- **Payload**: `payload_soil_moisture_t { uint16_t raw_adc; uint8_t moisture_pct; }`
- Sem ACK wait (device dorme, não espera confirmação)
- Sem pareamento persistente (broadcast não precisa de peer)

### Configuração WiFi (WiFiManager)

- SSID do portal: `ESP8266_SoilMoisture` / `password123`
- Campos: SSID, senha, device name (padrão: `Soil Moisture`), intervalo (padrão: 10 min)
- Timeout do portal: 2 min
- Persistência em EEPROM

### Hardware

| Componente | GPIO | Notas |
|------------|------|-------|
| Sensor capacitivo | A0 | ADC 0-1024 |
| LED (built-in) | D4/GPIO2 | Active LOW |
| Botão config | D3/GPIO0 | GND no boot abre portal |
| Deep sleep wake | D0/GPIO16 → RST | Ligar GPIO16 ao pino RST |

### Consumo Estimado

| Fase | Corrente | Duração |
|------|----------|---------|
| Boot + WiFi conectar | ~80mA | ~1.5s |
| Leitura ADC | ~5mA | ~50ms |
| ESP-NOW send | ~170mA | ~50ms |
| Deep sleep | ~20µA | 600s (10 min) |

**Média**: ~0.4mA → 2000mAh ≈ 5000h (~7 meses) sem recarga

### Estrutura de Arquivos

```
clients/esp8266_soil_moisture/
├── .gitignore
├── build.sh
├── flash.sh
├── flash.ps1
├── monitor.sh
├── platformio.ini
├── SPEC.md
├── README.md
├── include/
│   └── config.h
└── src/
    └── main.cpp
```

Sem `pages.h` — não há dashboard runtime. Configuração via WiFiManager apenas.

### Protocolo — Shared

Adicionar em `shared/src/espnow_protocol.h`:

```c
SENSOR_TYPE_SOIL_MOISTURE = 12,

typedef struct __attribute__((packed)) {
    uint16_t raw_adc;
    uint8_t moisture_pct;
} payload_soil_moisture_t;
```

### Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16

### Regras Específicas

1. Broadcast obrigatório (ESP8266→ESP32, regra 18)
2. `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes de `esp_now_init` (regra 21)
3. Deep sleep via `ESP.deepSleep(interval_us, WAKE_RF_DEFAULT)`
4. Sem `delay()` no active window (todo bloqueante fica antes do sleep)
5. WiFi timeout de 5s — se não conectar, vai dormir e tenta de novo
6. LED pisca rápido (100ms) ao enviar, apaga antes do sleep
7. RTC memory não usada (design simplificado, sem estado entre sleeps)
