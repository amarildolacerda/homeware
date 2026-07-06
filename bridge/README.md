# ESP-HA Bridge

Bridge em Python que conecta sensores ESP8266 ao Home Assistant via MQTT Discovery.

## Como Funciona

1. Os **clients ESP8266** se registram no bridge via gateway ESP-NOW ou HTTP/UDP direto
2. O bridge publica **entidades MQTT Discovery** no formato do Home Assistant
3. O **Home Assistant** descobre e cria os dispositivos automaticamente

## Instalação Rápida

### Docker

```bash
docker compose up -d
```

Sobe Mosquitto, Home Assistant e o bridge.

### Raspberry Pi (curl)

```bash
curl -fsSL https://raw.githubusercontent.com/amarildolacerda/homeware/main/bridge/install.sh | bash
```

Personalizando:

```bash
curl -fsSL https://raw.githubusercontent.com/amarildolacerda/homeware/main/bridge/install.sh | MQTT_HOST=core-mosquitto MQTT_USER=seu_usuario MQTT_PASS=sua_senha HTTP_PORT=80 bash
```

### Raspberry Pi (sem curl)

```bash
git clone --depth 1 --branch main https://github.com/amarildolacerda/homeware.git /tmp/homeware
rsync -av /tmp/homeware/bridge/ /addons/bridge_python/
rm -rf /tmp/homeware
cd /addons/bridge_python && python3 -m venv venv && venv/bin/pip install -r requirements.txt
nohup venv/bin/python -m app.main > bridge.log 2>&1 &
```

## Instalação como Add-on HA

Veja [HA_INTEGRATION.md](HA_INTEGRATION.md).

## Device Types Suportados

| Tipo | O que aparece no HA | Exemplo |
|------|---------------------|---------|
| `onoff` | Switch (liga/desliga) | Relé |
| `temperature` | Temperatura + Umidade | Sensor DHT22 |
| `humidity` | Umidade (%) | Sensor de umidade |
| `gas` | Alarme (sim/não) + Nível de gás | Detector de gás |
| `rain` | Chuva (sim/não) + Nível | Sensor de chuva |
| `contact` | Porta (aberta/fechada) | Sensor magnético |
| `occupancy` | Presença | Sensor PIR |
| `light_sensor` | Luminosidade (lx) | LDR |
| `tanque` | Nível de água (%) | Boia |
| `dimmable` | Luz ajustável | Lâmpada dimerizável |
| `electricity` | Corrente (mA) | Medidor |
| `dht_gas` | Temperatura + Umidade + Gás | DHT22 + MQ-2 |

## Variáveis de Ambiente

| Variável | Padrão | O que faz |
|----------|--------|-----------|
| `MQTT_HOST` | `core-mosquitto` | Endereço do Mosquitto |
| `MQTT_PORT` | `1883` | Porta do Mosquitto |
| `MQTT_USER` | vazio | Usuário MQTT |
| `MQTT_PASS` | vazio | Senha MQTT |
| `HTTP_PORT` | `80` | Porta do painel web |
| `LOG_LEVEL` | `info` | Nível do log |

## Painel Web

Acesse `http://<ip_do_bridge>:80/` para ver os dispositivos em tempo real.

## API

| O que faz | Rota |
|-----------|------|
| Health check | `GET /api/ping` |
| Listar devices | `GET /api/devices` |
| Detalhes de um device | `GET /api/devices/<id>` |
| Atualizar device | `PATCH /api/devices/<id>` |
| Status do bridge | `GET /api/gateway/info` |
| Registrar device (gateway) | `POST /api/device/register` |
| WebSocket (tempo real) | `GET /ws` |
| Docs da API | `GET /docs` |

## Clientes ESP8266

Veja a pasta `clients/` para exemplos de firmware Arduino.
