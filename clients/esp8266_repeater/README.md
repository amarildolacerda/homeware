# ESP-NOW Repeater

Extende o alcance do ESP-NOW entre clientes e o gateway.

## Funcionamento

- Cliente envia para o MAC do repeater
- Repeater reenvia para o gateway
- Gateway responde para o repeater
- Repeater reenvia a resposta para o cliente (via cache de sequência)

Totalmente transparente — o cliente e o gateway não precisam saber da existência do repeater.

## Setup

1. Grave o firmware no ESP (serial)
2. Na primeira vez, o ESP abre um portal WiFi `Repeater-Config`
3. Conecte-se a ele e configure:
   - Sua rede WiFi (SSID/senha)
   - MAC do gateway (`AA:BB:CC:DD:EE:FF`)
4. Salve — o repeater reinicia e começa a encaminhar
5. Para reconfigurar, use serial `w` ou segure o pino D3 (GPIO0) no boot

### ESP-01S

Para ESP-01S, use o environment `esp8266_esp01s`:
```bash
pio run -e esp8266_esp01s
```
WiFi e gateway MAC são configurados via build flags em `platformio.ini` (STATIC_WIFI).

## Configuração nos Clientes

No cliente (ex: lampada), configurar `REPEATER_MAC` com o MAC do repeater:

```cpp
#define REPEATER_MAC "AA:BB:CC:DD:EE:FF"
```

Ou via WiFiManager (campo repeater_mac).

## Dashboard

```
http://<repeater_ip>:80/
```

Página única com: gateway MAC, clientes conectados, pacotes RX/TX, canal, RSSI, uptime.

```
http://<repeater_ip>:80/api/status
```

JSON com todos os dados para integração.

## Leds

| Estado | Comportamento |
|--------|--------------|
| Piscando lento (2s) | Ativo, esperando pacotes |
| Piscando rápido | Tráfego recente |

## Serial Commands

| Key | Action |
|-----|--------|
| `s` | Status (clientes, pacotes, gateway) |
| `w` | Reconfigurar WiFi + gateway MAC (portal) |
| `m` | Monitor toggle (log cada pacote roteado) |
| `c` | Zerar contadores RX/TX |
| `r` | Restart |
| `h` / `?` | Help |
