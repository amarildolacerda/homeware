# ESP8266 Lampada (Relé com Timer + Alexa)

Firmware for ESP8266 (Wemos D1 Mini) relay control with ESP-NOW sensor protocol,
agendamento autônomo (timer liga/desliga) e integração Alexa (Espalexa).

## Hardware

### D1 Mini (padrão)

| Component | Pin | Notes |
|-----------|-----|-------|
| Relay     | D2 (GPIO4) | Active HIGH |
| LED (built-in) | D4 (GPIO2) | Active LOW, status indicator |

### ESP-01S

| Component | Pin | Notes |
|-----------|-----|-------|
| Relay     | GPIO0 | Active HIGH |
| Button    | GPIO2 | INPUT_PULLUP |
| LED       | — | Não disponível (GPIO2 usado para botão) |

Build: `pio run -e esp8266_esp01s`

### Status LED

| State | Blink |
|-------|-------|
| WiFi Config Portal | Solid ON |
| WiFi disconnected | Fast blink (250ms) |
| Not paired | Slow blink (500ms) |
| Paired + connected | OFF |

## Serial Commands (115200 baud)

| Key | Action |
|-----|--------|
| `l` | Toggle relay |
| `0` | Turn off |
| `1` | Turn on |
| `s` | Device status |
| `p` | Reset pairing |
| `u` | OTA info |
| `a` | Alexa device info |
| `h` / `?` | Help |
| `r` | Restart |

## Repeater (ESP-NOW Range Extender)

Para locais distantes do gateway, configure um MAC de repeater:

**Opção 1 — `config.h`:**
```cpp
#define REPEATER_MAC "AA:BB:CC:DD:EE:FF"
```

**Opção 2 — WiFiManager:**
No portal de configuração, preencha o campo `Repeater MAC`.

Quando o repeater está configurado, o cliente pula a etapa de pareamento e envia todos os pacotes direto para o repeater, que encaminha ao gateway.

## Alexa (UPnP / Fauxmo)

Emula um dispositivo Belkin Wemo, detectável pela Alexa na rede local.

**Comandos de voz:**
- "Alexa, ligue **Luz Lampada**"
- "Alexa, desligue **Luz Lampada**"

O nome do dispositivo é o mesmo configurado via WiFiManager (padrão: `Luz Lampada`).

## Web Interface

- `http://<ip>:80/` — Dashboard with on/off button + estado (compacto, detalhes expansíveis)
- `http://<ip>:80/api/state` — Full device JSON
- `http://<ip>:80/api/relay` — GET (state) / POST (`{"state":true/false}`)
- `http://<ip>:80/api/pin` — Direct GPIO control

## Timer (Agendamento Autônomo)

O client liga/desliga o relé de forma autônoma, sem depender do gateway ou da nuvem,
usando até `MAX_TIMERS` agendamentos. O disparo ocorre no minuto exato (`hora*60+minuto`),
uma única vez por minuto, e respeita a máscara de dias.

> **Dependência de relógio:** o timer só funciona após o relógio ser sincronizado. O
> client recebe o horário UTC do gateway via `ESPNOW_MSG_TIME_SYNC` (broadcast periódico)
> e aplica o `timezone_offset` (padrão `-3`). Se o gateway não estiver com NTP sincronizado,
> o relógio fica em 0 e o timer não dispara.

**`days_mask`** (bitmask, `1 << wday` onde `0=Dom … 6=Sáb`; `0` = todos os dias):

| Valor | Significado |
|-------|-------------|
| `0`   | Todos os dias |
| `1`   | Domingo |
| `2`   | Segunda |
| `4`   | Terça |
| `8`   | Quarta |
| `16`  | Quinta |
| `32`  | Sexta |
| `64`  | Sábado |

Combinações: ex. `62` = Seg–Sáb (`2+4+8+16+32`), `65` = Dom+Sáb (`1+64`).

### API

- `GET /api/timers` — lista os timers:
  ```json
  {"timers":[{"hour":18,"minute":30,"action":1,"days_mask":0,"enabled":true}]}
  ```
- `POST /api/timers` — substitui todos os timers (array completo):
  ```json
  {"timers":[{"hour":18,"minute":30,"action":1,"days_mask":0,"enabled":true},
             {"hour":7,"minute":0,"action":0,"days_mask":62,"enabled":true}]}
  ```
- `GET /api/timer/next` — próximo disparo calculado:
  ```json
  {"next_epoch":1708201800,"next_action":1}
  ```

`action`: `1` = ligar relé, `0` = desligar. Os timers persistem em EEPROM.

## OTA

```
pio run -t upload --upload-port <ip>
```

## Build

```bash
pio run           # D1 Mini (padrão)
pio run -e esp8266_esp01s  # ESP-01S
```

## First Flash (Serial)

```bash
pio run -t upload --upload-port <COM_PORT>
```
