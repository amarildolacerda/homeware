# ESP8266 Lampada (ON/OFF Client)

Firmware for ESP8266 (Wemos D1 Mini) relay control with ESP-NOW sensor protocol.

## Hardware

| Component | Pin | Notes |
|-----------|-----|-------|
| Relay     | D2 (GPIO4) | Active HIGH |
| LED (built-in) | D4 (GPIO2) | Active LOW, status indicator |

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

## Alexa (UPnP / Fauxmo)

Emula um dispositivo Belkin Wemo, detectável pela Alexa na rede local.

**Comandos de voz:**
- "Alexa, ligue **Luz Lampada**"
- "Alexa, desligue **Luz Lampada**"

O nome do dispositivo é o mesmo configurado via WiFiManager (padrão: `Luz Lampada`).

## Web Interface

- `http://<ip>:8080/` — Dashboard with on/off button + status
- `http://<ip>:8080/api/state` — Full device JSON
- `http://<ip>:8080/api/relay` — GET (state) / POST (`{"state":true/false}`)
- `http://<ip>:8080/api/pin` — Direct GPIO control
- `http://<ip>:8080/api/ota` — Firmware upload

## OTA

```
pio run -t upload --upload-port <ip>
```

Or upload `.bin` via web dashboard.

## Build

```bash
pio run
```

## First Flash (Serial)

```bash
pio run -t upload --upload-port <COM_PORT>
```
