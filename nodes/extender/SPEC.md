# Spec: ESP-NOW Repeater

## Visão Geral

Extensor de alcance ESP-NOW. Clientes enviam para o MAC do repeater, que reenvia ao gateway. Gateway responde para o repeater, que reenvia ao cliente. Totalmente transparente — cliente e gateway não precisam saber da existência do repeater.

## Fluxo da Mensagem

```
Cliente ──▶ Repeater ──▶ Gateway
           ◀───────────────
     ◀─────
```

### Direção Cliente → Gateway

1. Cliente envia pacote ESP-NOW para o MAC do repeater
2. Repeater recebe em `espnow_recv_cb`, identifica origem como client (aprende MAC)
3. Cacheia `sequence` do pacote (para roteamento de resposta)
4. Reenvia para o gateway
5. Incrementa `s_forwarded`

### Direção Gateway → Cliente

1. Gateway envia pacote (ACK, COMMAND, etc.) para o repeater
2. Repeater identifica origem como gateway (`mac_is_equal(mac, s_gateway_mac)`)
3. Busca `sequence` no cache para encontrar o client de destino
4. Se encontrado: reenvia unicast para o client específico
5. Se não encontrado: reenvia broadcast para todos os clients conhecidos

## Componentes Funcionais

### Aprendizado de Clientes

- MACs de clientes são aprendidos dinamicamente ao receber pacotes
- Armazenados em `s_client_peers[MAX_PEERS][6]` (MAX_PEERS = 10)
- Gateway é identificado por comparação com `s_gateway_mac` configurado
- Adiciona clientes como peers ESP-NOW via `esp_now_add_peer`

### Cache de Sequência

- `SEQ_CACHE_SIZE = 16` entradas
- Cada entrada: `sequence`, `mac[6]`, `timestamp`
- Usado para rotear respostas do gateway ao client correto
- Entrada é consumida após uso (`time = 0`)

### Dashboard Web (porta 80)

- `GET /` — dashboard com: gateway MAC, clientes conectados, pacotes RX/TX, canal, RSSI, uptime
- `GET /api/status` — JSON completo para integração

### Terminal Serial (115200 baud)

| Tecla | Ação |
|-------|------|
| `s` | Status (clientes, pacotes, gateway) |
| `w` | Reconfigurar WiFi + gateway MAC (portal) |
| `m` | Monitor toggle (log de cada pacote roteado) |
| `c` | Zerar contadores RX/TX |
| `r` | Restart |
| `h` / `?` | Help |

## Configuração

1. Primeira vez: abre portal WiFi `Repeater-Config` / `password123`
2. Conecte-se e configure: SSID/senha WiFi + MAC do gateway
3. Salva em EEPROM
4. Para reconfigurar: serial `w` ou pino GPIO0 no boot

## Regras Importantes

1. Gateway MAC é obrigatório — sem ele o repeater não encaminha
2. WiFi deve estar no mesmo canal do gateway (ESP-NOW canal 1)
3. O repeater não deve processar mensagens, apenas encaminhar
4. LED pisca lento (2s) quando ativo, mais rápido se há tráfego recente (opcional)

## Hardware

- D1 Mini (ESP-12E/F)
- LED status: GPIO2 (D4 - builtin)

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.16
