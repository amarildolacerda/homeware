# SPEC_TCP — Protocolo TCP (Hub ↔ Nodes TCP)

Especificação do protocolo TCP/HTTP entre o hub e nodes que usam `TCP_ENABLED`.
Nodes TCP comunicam via HTTP REST (não ESP-NOW), com descoberta via UDP broadcast.

## 1. Geral

- Transporte: **HTTP REST** (node→hub: POST/GET; hub→node: POST/GET via push ou polling).
- Descoberta: **UDP broadcast na porta 5000** (`TCP_UDP_PORT`).
- HTTP port: **80** (`TCP_HTTP_PORT`).
- Command TTL: **30s** (`TCP_COMMAND_TTL_MS`).
- Max pending commands por device: **10** (`TCP_MAX_PENDING_COMMANDS`).

## 2. Descoberta UDP

Node inicia com `MSG_GW_DISCOVER` (broadcast UDP porta 5000):
```c
struct tcp_gw_discover_t {
    uint8_t  msg_type;      // MSG_GW_DISCOVER (0x0A)
    uint8_t  sensor_type;   // sensor_type_t
    char     device_name[32];
};
```

Hub responde com `MSG_GW_ANNOUNCE` (unicast para o node):
```c
struct tcp_gw_announce_t {
    uint8_t  msg_type;      // MSG_GW_ANNOUNCE (0x09)
    uint8_t  fw_version[4];
    char     hub_ip[16];    // IP do hub
    uint16_t hub_port;      // HTTP port (80)
};
```

Node aprende o IP do hub e começa a registrar via HTTP.

## 3. Registro (Node → Hub)

**POST /node/register**

Body:
```json
{
  "device_id": "agri_XXXXXX_lamp_0",
  "device_name": "Lâmpada Sala",
  "fw_version": "1.2.8",
  "sensor_type": 8,
  "mac": "AA:BB:CC:DD:EE:FF",
  "client_chip": 0
}
```

| Campo | Tipo | Obrigatório | Descrição |
|-------|------|-------------|-----------|
| device_id | string | ✅ | ID único do node |
| device_name | string | ✅ | Nome legível (max 32 bytes) |
| fw_version | string | ✅ | Versão do firmware |
| sensor_type | int | ✅ | Tipo do sensor (1–11) |
| mac | string | Opcional | MAC WiFi do node; se ausente, pseudo-MAC derivada do IP |
| client_chip | int | Opcional | 0=ESP8266, 1=ESP32, 0xFF=desconhecido |

Resposta:
```json
{
  "status": "ok",
  "assigned_slot": 5,
  "device_id": "agri_XXXXXX_lamp_0"
}
```

### MAC pseudo-aleatório
Se o node não envia `mac`, o hub gera um pseudo-MAC a partir do IP do client:
- Byte 0: `0x02` (locally-administered, unicast)
- Byte 1-2: `0x00`
- Byte 3-5: IP empacotado

## 4. Estado (Node → Hub)

**POST /node/state**

Node publica estado periodicamente (a cada `m_state_interval_ms`) e logo após registro:
```json
{
  "device_id": "agri_XXXXXX_lamp_0",
  "state": true,
  "ip": "192.168.1.100",
  "free_heap": 32000
}
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| device_id | string | ID do node |
| state | bool/uint8 | Estado do relé (true/false ou 1/0) |
| relay_state | bool/uint8 | Alternativa legada para `state` |
| ip | string | IP do node (para push) |
| free_heap | int | Memória livre |
| temperature | float | Para SENSOR_TYPE_TEMP_HUM |
| humidity | float | Para SENSOR_TYPE_TEMP_HUM |
| gas_level | int | Para SENSOR_TYPE_GAS |
| alarm | int | Para SENSOR_TYPE_GAS |

## 5. Heartbeat (Node → Hub)

**POST /node/heartbeat**

Node envia heartbeat periodicamente:
```json
{
  "device_id": "agri_XXXXXX_lamp_0"
}
```

Atualiza `last_seen` e `online=true` no registry.

## 6. Comando (Hub → Node)

Node consulta comandos pendentes via polling:

**GET /node/command/{device_id}**

Resposta (com comando pendente):
```json
{
  "command": "on",
  "slot": 5
}
```

Resposta (sem comando):
```json
{}
```

### Comandos suportados
| Comando | Descrição |
|---------|-----------|
| `on` | Liga relé (command=0x01) |
| `off` | Desliga relé (command=0x00) |
| `restart` | Reinicia o node |

### Push (Hub → Node)
Quando o hub conhece o IP do node, envia comando diretamente via HTTP:
- **POST http://{node_ip}:80/api/relay** com body `{"state":true}` ou `{"state":false}`
- Retry a cada 1s (`TCP_PUSH_RETRY_INTERVAL_MS`)
- TTL 30s
- Se push falha, node pode descobrir via polling

## 7. Timeout e Online

- `SENSOR_TIMEOUT_MS = 300000` (5 min): sem heartbeat/state → `online=false`
- `last_seen` atualizado a cada heartbeat/state/registro

## 8. Implementação

### Arquivos
- `hub/src/tcp_radio_handler.cpp` — implementação completa
- `hub/src/tcp_radio_handler.h` — classe `TcpRadioHandler : public RadioInterface`
- `shared/src/tcp_protocol.h` — structs e constantes

### Flags de compilação
- `TCP_ENABLED` — habilita TCP radio handler
- `ESPNOW_ENABLED` — habilita ESP-NOW radio handler
- Ambos podem estar habilitados simultaneamente (modo Hybrid)

### RadioInterface
`TcpRadioHandler` implementa `RadioInterface` para integração com `RadioManager`:
- `init()` — registra endpoints HTTP e UDP server
- `loop()` — processa UDP discover, limpa comandos expirados, envia push
- `send_command()` — enfileira comando + push direto se IP conhecido
- `send_restart()` — enfileira comando de restart
- `is_ready()` — sempre retorna true (HTTP sempre disponível)

### Dashboard
Cards de configuração colapsáveis (accordion):
- **Hub** — informações do hub
- **Modo** — operação (Terminal/AP/Hybrid)
- **WiFi** — credenciais
- **MQTT** — broker

## 9. Regras importantes

1. UDP/5000 só é necessário quando `TCP_ENABLED` — nodes TCP precisam descobrir o hub
2. Node deve publicar `state` periodicamente e logo após registro
3. `state` aceita bool (`true/false`) e uint8 (`1/0`), além de `relay_state` legado
4. `SENSOR_TYPE_LIGHT` (9) e `SENSOR_TYPE_ONOFF` (8) usam o mesmo handler de estado
5. Hub faz max 1 tentativa HTTP por ciclo de loop (não bloqueia)
6. Comandos são idempotentes (on/off) — polling repetido não altera estado duas vezes
