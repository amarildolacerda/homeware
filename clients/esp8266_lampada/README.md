# ESP8266 Lampada (ON/OFF Client)

Firmware for ESP8266 (Wemos D1 Mini) relay control with ESP-NOW sensor protocol.

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
