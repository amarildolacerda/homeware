# Arduino Nano LoRa Switch (ON/OFF)

Node relay para Arduino Nano (ATmega328P) com módulo Seeed Studio Grove LoRa 868MHz.
Comunica com o hub existente via LoRa usando driver UART direto (sem RadioHead).
O módulo Seeed opera com firmware de fábrica (ATMega168 bridge SPI) — sem necessidade de reflash.

## Hardware

### Componentes

| Componente | Pino | Notas |
|------------|------|-------|
| Seeed Grove LoRa 868MHz | D2 (RX), D3 (TX) | SoftwareSerial 9600 baud |
| Relay | D5 | Active HIGH (configurable) |
| Button | D6 | INPUT_PULLUP, active LOW |
| LED | D4 | Active LOW |

### Módulo Seeed Grove LoRa

O módulo já vem com firmware de fábrica (bridge SPI via ATMega168). O Arduino Nano se comunica via UART enviando comandos 'W'/'R' para leitura/escrita direta dos registradores SX1276 — sem RadioHead, sem header extra.

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

O node envia `lora_frame_t` cru (sem header adicional) via bridge SPI do módulo Seeed.
O hub também envia `lora_frame_t` cru — protocolo idêntico em ambas as pontas.

```
TX: [lora_frame_t] → bridge SPI → SX1276 → LoRa
RX: SX1276 → bridge SPI → [lora_frame_t] cru
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

## EEPROM Layout

| Offset | Tamanho | Conteúdo |
|--------|---------|----------|
| 200 | 1 byte | Relay state |
| 201 | 1 byte | Paired flag |
| 202-207 | 6 bytes | My MAC (signature) |

## Recursos

| Recurso | Valor |
|---------|-------|
| RAM | ~923 / 2048 bytes (45%) |
| Flash | ~9KB / 30KB (29%) |
| Platform | ATmega328P (Arduino Nano) |
| Framework | Arduino |
| LoRa lib | Nenhuma (driver UART direto via bridge SPI) |
