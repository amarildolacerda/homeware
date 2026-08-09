# Arduino Nano LoRa Switch (ON/OFF)

Node relay para Arduino Nano (ATmega328P) com módulo Seeed Studio Grove LoRa 868MHz.
Comunica com o hub existente via LoRa usando RadioHead (`RH_RF95`) via SoftwareSerial.
O módulo Seeed opera com firmware de fábrica — sem necessidade de reflash.

## Hardware

### Componentes

| Componente | Pino | Notas |
|------------|------|-------|
| Seeed Grove LoRa 868MHz | D2 (RX), D3 (TX) | SoftwareSerial 9600 baud |
| Relay | D5 | Active HIGH (configurable) |
| Button | D6 | INPUT_PULLUP, active LOW |
| LED | D4 | Active LOW |

### Módulo Seeed Grove LoRa

O módulo já vem com firmware RadioHead de fábrica. O Arduino Nano usa a mesma biblioteca (`RH_RF95`) — funciona direto, sem reflash.

**Pinagem Grove → Arduino Nano:**
| Grove Pin | Arduino Nano |
|-----------|-------------|
| TX | D2 (RX SoftwareSerial) |
| RX | D3 (TX SoftwareSerial) |
| VCC | 5V |
| GND | GND |

### Status LED

| Estado | LED |
|--------|-----|
| Pareado + relé ON | Aceso (LOW) |
| Pareado + relé OFF | Apagado (HIGH) |
| Em pareamento | Pisca 250ms |

## Serial Commands (115200 baud)

| Tecla | Ação |
|-------|------|
| `l` | Alterna relé (toggle) |
| `s` | Status (device, relay, pareado, RSSI, uptime, RX/TX) |
| `p` | Força re-pareamento |
| `h` / `?` | Ajuda |
| `r` | Reinicia |

## Protocolo LoRa

O node envia `lora_frame_t` encapsulado em pacote RadioHead (header de 4 bytes: TO, FROM, ID, FLAGS).

O hub detecta automaticamente o header RadioHead e o remove antes de processar `lora_frame_t`.

```
No ar:  [TO=FF][FROM=00][ID=01][FLAGS=00][lora_frame_t...]
Hub:    detecta header → remove 4 bytes → processa lora_frame_t normalmente
```

### Mensagens enviadas

| Tipo | msg_type | Payload | Quando |
|------|----------|---------|--------|
| Pair Request | 0x02 | sensor_type + device_name | Boot, retry 5s |
| Sensor Data | 0x01 | relay_state | Mudança + periódico 60s |
| Heartbeat | 0x05 | vazio | 60s |

### Mensagens recebidas

| Tipo | msg_type | Payload | Ação |
|------|----------|---------|------|
| Pair Response | 0x03 | assigned_slot | Salva slot, marca pareado |
| Command | 0x07 | command | 0x01=ON, 0x00=OFF, 0xFF=restart |
| NAK | 0x0D | reason | Log |

### Device ID

`avr_<signature>` — 3 bytes da signature do ATmega328P (único por chip).

## Build

```bash
cd nodes/onoff-lora-avr
./build.sh
# ou diretamente:
pio run
```

## Flash

```bash
./flash.sh /dev/ttyUSB0
# ou:
pio run --target upload --upload-port /dev/ttyUSB0
```

## Monitor

```bash
./monitor.sh /dev/ttyUSB0
# ou:
pio device monitor --port /dev/ttyUSB0
```

## Integração com Hub

O node aparece automaticamente no sensor registry do hub após pareamento.
O hub encaminha o estado do relé para Home Assistant via MQTT.

**Requisitos:**
- Hub com LoRa habilitado (`LORA_ENABLED` na config)
- Módulo LoRa no hub sintonizado em 868MHz
- Parâmetros idênticos: SF10, BW125kHz, CR 4/7, Sync Word 0x12
- Hub com detecção de header RadioHead (já implementado)

## EEPROM Layout

| Offset | Tamanho | Conteúdo |
|--------|---------|----------|
| 200 | 1 byte | Relay state |
| 201 | 1 byte | Paired flag |
| 202-207 | 6 bytes | My MAC (signature) |

## Recursos

| Recurso | Valor |
|---------|-------|
| RAM | ~900 / 2048 bytes (44%) |
| Flash | ~12KB / 30KB (40%) |
| Platform | ATmega328P (Arduino Nano) |
| Framework | Arduino |
| LoRa lib | RadioHead (`RH_RF95`) |
