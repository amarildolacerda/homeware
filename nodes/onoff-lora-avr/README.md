# Arduino Nano LoRa Switch (ON/OFF)

Node relay para Arduino Nano (ATmega328P) com módulo Seeed Studio Grove LoRa 868MHz.
Comunica com o hub existente via LoRa usando o protocolo `lora_frame_t` (mesmo formato dos nodes ESP32).

## Hardware

### Componentes

| Componente | Pino | Notas |
|------------|------|-------|
| Seeed Grove LoRa 868MHz | D2 (RX), D3 (TX) | SoftwareSerial 9600 baud |
| Relay | D5 | Active HIGH (configurable) |
| Button | D6 | INPUT_PULLUP, active LOW |
| LED | D4 | Active LOW |

### Módulo Seeed Grove LoRa

O módulo vem com ATMega168 de fábrica que precisa ser **regravado** com o firmware `lora_bridge`.

#### Por que regravar?

O firmware de fábrica usa a biblioteca **RadioHead** (`RH_RF95`), que adiciona overhead ao pacote:

```
Fábrica (RadioHead):  [preamble][sync][header 4B][payload][CRC]
Esperado pelo hub:     [lora_frame_t cru — msg_type + sequence + sensor_id + ...]
```

Quando o Arduino Nano envia `MSG_PAIR_REQUEST` (0x02) via RadioHead, os primeiros bytes no ar são header do RadioHead, não o `msg_type`. O hub recebe via `Sandeepmistry/LoRa` (raw SPI) e interpreta esses bytes como `lora_frame_t` → `msg_type` errado → frame descartado.

**Exemplo:**
```
Node envia:      [0x02][0x01 0x00][AA BB CC DD EE FF]...  ← lora_frame_t correto
RadioHead envia: [0x00][0xFF][0x01][0x02][0x02]...        ← header RadioHead + payload
                       ↑ hub vê 0x00 como msg_type → descarta
```

**Solução escolhida:** reescrever o ATMega168 para **raw SPI pass-through** — recebe bytes via UART, repassa cru ao SX1276 via SPI, sem overhead. São ~30 linhas de C que configuram o SX1276 diretamente via registradores.

**Alternativa descartada:** usar RadioHead no Arduino Nano e adaptar o hub para parserar o header RadioHead — frágil, depende de versão da lib, mistura de abstrações.

**Gravação via ISP:**
```bash
# Usando Arduino Nano como ISP, ou gravador USBASP
# Carregar firmware/lora_bridge/lora_bridge.ino via IDE Arduino
```

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

Usa o mesmo `lora_frame_t` do hub — plug-and-play com o sensor registry existente.

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

## Firmware ATMega168 (módulo Seeed)

O firmware `firmware/lora_bridge/lora_bridge.ino` transforma o ATMega168 em bridge SPI raw:

- Recebe bytes via UART (do Arduino Nano)
- Repassa ao SX1276 via SPI (sem overhead RadioHead)
- Configuração idêntica ao hub: 868MHz, SF10, BW125kHz, CR 4/7

**UART Protocol:**
- `T` + len_hi + len_lo + [data] → TX packet
- `R` → Habilitar RX contínuo
- `?` → Status
- Resposta: `D` + len + [data] (RX) ou `T` (TX OK) ou `E` (erro)

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
| RAM | 865 / 2048 bytes (42%) |
| Flash | 8174 / 30720 bytes (27%) |
| Platform | ATmega328P (Arduino Nano) |
| Framework | Arduino |
| LoRa lib | Nenhuma (raw SPI) |
