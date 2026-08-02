# AgriSense TCP Node

Node TCP via WiFi HTTP + UDP discovery para o hub AgriSense.

## Features

- WiFi não-bloqueante (WiFiManager com AP customizado)
- Descoberta automática do hub via UDP broadcast
- Comunicação HTTP REST com o hub
- OTA (Over-The-Air) update
- Console/telnet remoto
- Dashboard web responsivo
- Suporte a múltiplos tipos de sensor

## Hardware

- ESP8266 (D1 Mini)
- Sensores: DHT21/22 (temp/umidade), MQ-2 (gás), relé (ON/OFF), LDR (luz)

## WiFi Setup

1. Conecte-se à rede WiFi **AgriSense-TCP-Setup** (senha: `agrisense`)
2. Abra o captive portal em `192.168.4.1`
3. Configure SSID, senha e nome do dispositivo
4. O node salvará as credenciais na EEPROM

## Hub Discovery

O node descobre o hub automaticamente via UDP broadcast na porta 5000:
1. Envia `GW_DISCOVER` a cada 10s (max 20 tentativas)
2. Recebe `GW_ANNOUNCE` do hub com IP e porta
3. Fallback: IP configurado na EEPROM ou default (`192.168.1.100`)

## API Endpoints (node como servidor)

| Endpoint | Método | Descrição |
|----------|--------|-----------|
| `/api/state` | GET | Estado do node (sensores, uptime, memória) |
| `/api/settings` | GET/POST | Nome do dispositivo |
| `/api/wifi` | GET/POST | Credenciais WiFi |
| `/api/ota` | POST | Upload de firmware |
| `/api/restart` | POST | Reiniciar node |

## Hub Endpoints (node como cliente)

| Endpoint | Método | Descrição |
|----------|--------|-----------|
| `/node/register` | POST | Registro do node no hub |
| `/node/state` | POST | Envio de estado dos sensores |
| `/node/heartbeat` | POST | Heartbeat periódico |
| `/node/command/{id}` | GET | Consulta de comandos pendentes |

## Serial Commands

| Comando | Descrição |
|---------|-----------|
| `h` | Ajuda |
| `s` | Status (IP, hub, uptime, sensores) |
| `r` | Reiniciar |
| `l` | Listar configurações |
| `d` | Detalhes de depuração |
| `u` | Status OTA |

## Build

```bash
./build.sh          # Compilar
./flash.sh          # Flash serial (default: /dev/ttyUSB0)
./flash.sh -p /dev/ttyUSB1  # Flash serial (porta específica)
./flash.sh -o 192.168.1.100 # Flash OTA
./monitor.sh         # Monitor serial
```

## Configuração

Parâmetros em `include/config.h`:
- `STATE_UPDATE_INTERVAL`: intervalo de envio de estado (10s)
- `HEARTBEAT_INTERVAL`: intervalo de heartbeat (30s)
- `DISCOVERY_INTERVAL`: intervalo de discovery UDP (10s)
- `MAX_RETRIES`: máximo de retries HTTP (5)
- `HUB_IP_DEFAULT`: IP fallback do hub (192.168.1.100)

## EEPROM Layout

| Offset | Tamanho | Descrição |
|--------|---------|-----------|
| 0-47 | 48 | WiFi/Name (shared) |
| 96-111 | 16 | Hub IP |
| 112 | 1 | Hub IP válido (0x01) |
| 200 | 1 | Estado do relé |
