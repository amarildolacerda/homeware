# SPEC_ESPNOW — Protocolo ESP-NOW (Gateway ↔ Clients)

Especificação do protocolo de rádio entre o gateway ESP-NOW e os clients
(sensores/atuadores/repeaters). Referente a `FW_VERSION` atual do gateway.

## 1. Geral
- Projeto: https://github.com/amarildolacerda/esp-now
- Transporte: **ESP-NOW** (sem WiFi/IP entre client e gateway).
- Canal: **fixo em `ESP_NOW_CHANNEL` (1)** — regra crítica: todos os clients,
  repeaters e o gateway devem estar no **mesmo canal do AP**. Canal divergente
  quebra todo o ESP-NOW.
- Roles:
  - ESP8266: `ESP_NOW_ROLE_COMBO`.
  - ESP32: role nativo.
- MAC Alt: ESP-NOW usa MAC diferente do WiFi (bit 1 do byte 0 invertido,
  `mac[0] ^= 0x02`). Quem recebe `PAIR_RESPONSE` aprende o MAC alt do gateway
  pela source do frame. Para enviar unicast a um ESP8266, derivar o MAC alt do
  WiFi MAC.
- Payload máximo: `ESPNOW_MAX_PAYLOAD = 250` bytes.
- Versão de protocolo: `ESPNOW_PROTOCOL_VERSION = 1` (validada no RX).

## 2. Broadcast vs Unicast (revisado — causa raiz: canal do AP)

Dois modos de entrega existem: **unicast** (MAC específico) e **broadcast**
(`FF:FF:FF:FF:FF:FF`). O caminho correto depende de **onde o rádio está
sintonizado**, não do chip em si.

### Causa raiz (confirmada em espressif arduino-esp32 #8912 e QuickESPNow)
- ESP-NOW **só opera no canal em que o rádio está no momento**.
- Quando o receptor está **STA conectado a um AP**, o rádio muda para o canal
  do AP. Um unicast enviado em canal diferente é **dropado silenciosamente**
  (sem erro de retorno — `esp_now_send` retorna 0/"enfileirado OK").
- Se o receptor **não está associado a AP** (SoftAP apenas, ou
  `WiFi.disconnect()` + canal explícito), o rádio fica no canal ESP-NOW e o
  unicast funciona.
- Por isso: **quando o receptor (ex: gateway ESP32) está STA no AP do
  roteador, client ESP8266→ESP32 em unicast falha**; broadcast ainda chega.
  Se o gateway não estivesse conectado ao AP (modo autônomo, §16), o unicast
  funcionaria.

### Regras por cenário
- **ESP8266 ↔ ESP8266 (homogêneo), mesmo canal:**
  - Unicast funciona bem nos dois sentidos (o repeater envia unicast para
    seus clients peers em `process_forward_queue()`).
  - Broadcast também funciona. Unicast é preferível (menos tráfego/colisões).
- **Receptor STA conectado a AP (canal do AP):**
  - Quem envia para esse receptor deve usar **BROADCAST** (unicast cai se o
    canal do sender divergir do canal do AP). É o caso client ESP8266 →
    gateway ESP32 STA no roteador.
  - **ESP32→ESP8266**: se o ESP8266 também estiver no mesmo canal (ex: ambos
    no AP do gateway ou ambos no canal explícito), unicast funciona; se o
    ESP8266 estiver em canal divergente, usar broadcast.
- **Receptor sem AP (SoftAP / autônomo, canal explícito):**
  - Unicast funciona (validado em `test_espnow` e QuickESPNow exemplo com
    `WiFi.disconnect(false)` + `begin(canal)`).

### Regra prática (decisão ANTES do envio)
- **PREFERÊNCIA: unicast.** Sempre que os dois dispositivos que estão falando
  funcionam com unicast (mesmo canal, sem AP no meio causando drop), usar
  **UNICAST** — é mais eficiente (menos tráfego, menos colisões,ACK direcionado).
- **Destino ESP8266 + mesmo canal do sender → UNICAST** (preferido).
- **Destino conectado a AP em canal que o sender pode não estar → BROADCAST**
  (ex: client ESP8266 → gateway ESP32 STA no roteador).
- Resumo seguro: **use broadcast apenas quando o destino for um device STA no
  AP do roteador** (gateway ESP32) ou quando o canal divergir; em todos os
  demais casos (autônomos, mesmo canal explícito, ESP8266↔ESP8266) prefira
  unicast.
- O fallback "unicast→broadcast se `esp_now_send` der erro" é insuficiente:
  no ESP32 nativo `esp_now_send` retorna 0 mesmo quando o frame é dropado,
  então o fallback nunca dispara. Ver §14 (decisão por `client_chip` + canal).

### Regra prática
- **ESP8266 → ESP8266: sempre UNICAST** (funciona bem nos dois sentidos,
  inclusive gateway ESP8266 → client ESP8266 e repeater ESP8266 → client
  ESP8266). Unicast é preferível (menos tráfego/colisões).
- **ESP32 → ESP8266: UNICAST OK** (preferível).
- **ESP8266 → ESP32: BROADCAST obrigatório** (unicast é dropado pela
  coexistência rádio/WiFi).
- **ESP32 → ESP32: UNICAST OK.**
- Resumo: unicast sempre que o **destino for ESP8266**; broadcast apenas
  quando o destino for ESP32 e a origem for ESP8266.
- O fallback "unicast→broadcast se `esp_now_send` der erro" é insuficiente:
  no ESP32 nativo `esp_now_send` retorna 0 (enfileirado OK) mesmo quando o
  frame é dropado, então o fallback nunca dispara. A escolha do modo deve ser
  feita **antes** do envio, com base no par (tx_chip, rx_chip), não por erro
  de retorno. Ver §14 (decision por `client_chip`).

## 3. Frame e Header

Toda mensagem inicia com `espnow_header_t` (exceto PAIR_REQUEST/RESPONSE,
COMMAND, TIME_SYNC, GW_ANNOUNCE, GW_DISCOVER e ACK que têm structs próprias,
mas compartilham os campos iniciais `msg_type`/`sequence`/`mac`).

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;        // ESPNOW_PROTOCOL_VERSION
    uint8_t  msg_type;       // espnow_msg_type_t
    uint16_t sequence;       // 0..ESPNOW_SEQUENCE_MAX (65535), wrap
    uint8_t  sensor_mac[6];  // MAC do sensor de origem
    uint8_t  sensor_type;    // sensor_type_t
    uint8_t  battery_pct;
    int16_t  rssi;
    uint8_t  payload_len;
    uint8_t  payload[ESPNOW_MAX_PAYLOAD - 18];
} espnow_header_t;
```

`ESPNOW_HEADER_FIXED_SIZE` = tamanho do header sem `payload`.

## 4. Tipos de Mensagem (`espnow_msg_type_t`)

| Valor | Nome | Origem → Destino | Descrição |
|-------|------|------------------|-----------|
| 0x01 | `ESPNOW_MSG_SENSOR_DATA` | Client → Gateway | Leitura de sensor |
| 0x02 | `ESPNOW_MSG_PAIR_REQUEST` | Client → Gateway | Solicita pareamento |
| 0x03 | `ESPNOW_MSG_PAIR_RESPONSE` | Gateway → Client | Confirma slot atribuído |
| 0x04 | `ESPNOW_MSG_ACK` | Gateway → Client | Confirma recebimento/estado |
| 0x05 | `ESPNOW_MSG_HEARTBEAT` | Client → Gateway | Keep-alive |
| 0x06 | `ESPNOW_MSG_OTA_TRIGGER` | (reservado) | Disparo OTA |
| 0x07 | `ESPNOW_MSG_COMMAND` | Gateway → Client | Comando ON/OFF (atuadores) |
| 0x08 | `ESPNOW_MSG_TIME_SYNC` | Gateway → Client | Epoch UTC |
| 0x09 | `ESPNOW_MSG_GW_ANNOUNCE` | Gateway → Client | Anúncio presença gateway |
| 0x0A | `ESPNOW_MSG_GW_DISCOVER` | Repeater → Gateway | Descoberta de repeater |
| 0x0B | `ESPNOW_MSG_REPEATER_STATUS` | Repeater → Gateway | Status do repeater |

## 5. Tipos de Sensor (`sensor_type_t`)

| Valor | Nome | Payload | Estado exposto em `/api/sensors` |
|-------|------|---------|----------------------------------|
| 1 | `SENSOR_TYPE_TEMP_HUM` | `payload_temp_hum_t` | temperature, humidity |
| 2 | `SENSOR_TYPE_CONTACT` | `payload_contact_t` | contact, tamper |
| 3 | `SENSOR_TYPE_MOTION` | `payload_motion_t` | occupancy, duration |
| 4 | `SENSOR_TYPE_GAS` | `payload_gas_t` | gas_level, alarm |
| 5 | `SENSOR_TYPE_RAIN` | `payload_rain_t` | rain_level, rain_digital |
| 6 | `SENSOR_TYPE_TANK` | `payload_tank_t` | level_pct, distance_cm |
| 7 | `SENSOR_TYPE_DHT_GAS` | `payload_dht_gas_t` | temperature, humidity, gas_level, alarm |
| 8 | `SENSOR_TYPE_ONOFF` | `payload_onoff_t` | state |
| 9 | `SENSOR_TYPE_LIGHT` | `payload_onoff_t` | state |
| 10 | `SENSOR_TYPE_REPEATER` | `payload_repeater_status_t` | received, forwarded, client_count, channel, uptime_s, free_heap, ack_failures |

### Payloads (structs packed)

```c
payload_temp_hum_t   { float temperature; float humidity; }
payload_contact_t    { uint8_t contact_state; uint8_t tamper; }
payload_motion_t     { uint8_t motion_state; uint8_t occupancy_duration; }
payload_gas_t        { uint16_t gas_level; uint8_t alarm; }
payload_rain_t       { uint8_t rain_level; uint8_t rain_digital; }
payload_tank_t       { uint16_t level_pct; uint16_t distance_cm; }
payload_dht_gas_t    { float temperature; float humidity; uint16_t gas_level; uint8_t alarm; }
payload_onoff_t      { uint8_t state; }   // 0=OFF, 1=ON
payload_repeater_status_t {
    uint16_t received; uint16_t forwarded; uint8_t client_count;
    uint8_t channel; int16_t rssi; uint32_t uptime_s;
    uint16_t free_heap; uint8_t ack_failures;
}
```

## 6. Pareamento (Pairing)

### Fluxo
1. Gateway entra em modo pareamento via `POST /api/pair/start` (ou botão físico
   `PAIR_BUTTON_GPIO` / dashboard). `PAIRING_WINDOW_MS = 60000` (60s).
   No dashboard a janela exibida é `pairing_window_sec` de `/api/info` (180s
   no frontend, mas o backend expira em 60s — ver SPEC.md).
2. Client envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast):
   ```c
   espnow_pair_request_t {
       uint8_t  msg_type;      // 0x02
       uint16_t sequence;
       uint8_t  sensor_mac[6];
       uint8_t  sensor_type;   // sensor_type_t
       uint8_t  firmware_version[4];
       char     device_name[32]; // 32 bytes, null-terminated
   }
   ```
3. Gateway enfileira em `s_pending_pairs` (máx. `PENDING_PAIR_MAX = 5`).
   - Se MAC já existe no registry → reenvia `PAIR_RESPONSE` com slot existente.
   - Novo MAC → entra na fila (independente de `s_pairing_mode` no código atual,
     ver nota abaixo).
4. Em `espnow_handler_loop()`: consome fila, aloca slot livre
   (`sensor_registry_find_free_slot`, máx. `MAX_VIRTUAL_SENSORS = 64`),
   persiste em NVS e envia `ESPNOW_MSG_PAIR_RESPONSE` (broadcast):
   ```c
   espnow_pair_response_t {
       uint8_t  msg_type;      // 0x03
       uint16_t sequence;
       uint8_t  sensor_mac[6];
       uint8_t  gateway_mac[6]; // MAC alt do gateway
       uint8_t  status;         // PAIR_STATUS_*
       uint16_t assigned_slot;
   }
   ```
5. Se MQTT conectado → `mqtt_client_publish_discovery()` registra no bridge.

### Status de pareamento
```c
#define PAIR_STATUS_OK     0
#define PAIR_STATUS_FULL   1  // registry cheio
#define PAIR_STATUS_DENIED 2
```

### Fase 1 (atual) — pareamento sem restrição

- **Versão de protocolo: `ESPNOW_PROTOCOL_VERSION = 1`** (definido em
  `espnow_protocol.h`, validado no RX em `espnow_handler.cpp`). Toda mensagem
  da Fase 1 usa version=1.
- **O gateway aceita qualquer `ESPNOW_MSG_PAIR_REQUEST` sem exigir que esteja
  em modo de pareamento.** Se o MAC já existe → reenvia `PAIR_RESPONSE` com o
  slot existente; se novo → enfileira e aloca slot livre.
- Não há espera/whitelist: pediu pareamento, concede. Isso simplifica o fluxo
  via repeater (§13.2/§13.3) e o uso direto.
- O botão "Adicionar Sensor"/`POST /api/pair/start` ainda existe para o
  dashboard, mas **não é pré-requisito** para o gateway aceitar clients.
- **Fase futura (opcional):** pode passar a ser exigido entrar em modo de
  pareamento (janela) antes de aceitar `PAIR_REQUEST`, com politica de
  segurança/whitelist. Nesse momento, repetir a lógica de `s_pairing_mode`
  (hoje comentada em `espnow_handler.cpp:86`) e garantir que o repeater
  responda NAK quando o gateway não estiver em modo pareamento (§13.2).

### Flag de config: modo pareamento habilitado

Para controlar o comportamento acima sem rebuild, usar uma **flag persistente
em config/EEPROM** (`pairing_enabled`):

- Armazenada em EEPROM (novo offset após as configs WiFi, ex:
  `EEPROM_PAIRING_EN_OFFSET`). Default **false** na Fase 1 implícita, ou
  configurável via `/api/config`.
- Exposta em `/api/info` (ex: `pairing_required`) para o dashboard decidir a UI.
- Endpoint `GET/POST /api/config/pairing` para ligar/desligar:
  `{"enabled": true|false}`.
- **Impacto no dashboard**: se a flag estiver **desabilitada**, o botão
  "+ Adicionar Sensor" **não é exibido** (o gateway aceita pareamento livre,
  §Fase 1, então o botão seria redundante/enganoso). Se habilitada, o botão
  aparece e o gateway passa a exigir modo pareamento (Fase futura).
- O firmware respeita a flag: quando `pairing_enabled == false`, o gateway
  aceita qualquer `PAIR_REQUEST`; quando `true`, só aceita dentro da janela
  (`s_pairing_mode`).

> Decisão: a flag vive no config (não hardcode), para ligar/desligar o modo
> pareamento OTA/remotamente sem reflash.

> Nota: em `espnow_handler.cpp` o bloqueio `if (!s_pairing_mode) { ignore }`
> está comentado (`// test`), então o código já está em conforme com a Fase 1.
> Manter assim até a Fase futura.

### Campo device_name (regra 17)
- `device_name` tem **32 bytes** (compatível com `s_device_name[32]` dos clients
  e `virtual_sensor_t.name`).
- `EEPROM_SENSOR_SIZE = 48` bytes por sensor (nome ocupa 32 bytes no offset 9).
- Qualquer mudança nesse tamanho exige atualização simultânea de gateway e
  todos os clients.

> **Nota implementação**: em `espnow_handler.cpp` o bloqueio por
> `s_pairing_mode` em PAIR_REQUEST está comentado (`// test`), então clients são
> pareados mesmo fora da janela. Definir comportamento oficial antes de subir
> para `main`.

## 7. ACK

Gateway responde `ESPNOW_MSG_ACK` (broadcast) a cada SENSOR_DATA, HEARTBEAT,
REPEATER_STATUS (e a PAIR_REQUEST negado/cheio):
```c
espnow_ack_t {
    uint8_t  msg_type;   // 0x04
    uint16_t sequence;
    uint8_t  sensor_mac[6];
    uint8_t  status;     // PAIR_STATUS_*
    uint8_t  assigned_slot; // 0xFF se não aplicável
}
```
O client deve considerar reenvio se não receber ACK dentro de timeout
(máquina de estados non-blocking, regra 15).

## 8. Heartbeat e Online

- Client envia `ESPNOW_MSG_HEARTBEAT` periodicamente (referência
  `HEARTBEAT_INTERVAL_MS = 30000`).
- `SENSOR_TIMEOUT_MS = 300000` (5 min): sem heart/data → `online=false`.
- `last_rssi` / `free_heap` / `battery_pct` atualizados no registry.

## 9. Comando (Atuadores)

Gateway → Client (`SENSOR_TYPE_ONOFF` / `SENSOR_TYPE_LIGHT`):
```c
espnow_command_t {
    uint8_t  msg_type;   // 0x07
    uint16_t sequence;
    uint8_t  target_mac[6];
    uint8_t  command;    // 0=OFF, 1=ON
}
```
Disparado via `POST /api/sensor/{slot}/command {"state":0|1}`. Client deve
feedback imediato ao gateway (setar flag de reenvio, regra 14).

## 9.1 Comando de Restart via ESP-NOW (dashboard → client/repeater)

Permitir que o dashboard (gateway HTTP) envie um **restart** ao client ou
repeater alvo via ESP-NOW, sem depender de HTTP no client.

- Novo tipo de mensagem `ESPNOW_MSG_RESTART = 0x0C` (ou reuso de
  `ESPNOW_MSG_COMMAND` com `command = 0xFF` reservado para RESTART — decidir
  antes de implementar; preferencialmente tipo dedicado para clareza):
  ```c
  espnow_restart_t {
      uint8_t  msg_type;   // 0x0C
      uint16_t sequence;
      uint8_t  target_mac[6];
  }
  ```
- Gateway envia para o `target_mac` do sensor selecionado (unicast se for
  ESP8266, broadcast se for ESP32, conforme §2/§14). O client/repeater, ao
  receber, faz `ESP.restart()` (ou `delay`+reset) em modo non-blocking.
- **Endpoint dashboard**: `POST /api/sensor/{slot}/restart` — gateway localiza
  o `target_mac` pelo slot e dispara o frame ESP-NOW. Respostas: 200 ok, 404
  sensor not found, 400 tipo não suportado.
- Aplica-se a `SENSOR_TYPE_ONOFF`, `SENSOR_TYPE_LIGHT` e
  `SENSOR_TYPE_REPEATER` (qualquer client pareado com `target_mac` conhecido).
- O client deve confirmar o restart (ex: enviar um último HEARTBEAT/ACK antes
  de resetar) para o gateway marcar `online=false` e evitar estado órfão; se
  não confirmar, o gateway só detecta offline por `SENSOR_TIMEOUT_MS`.
- Repeter: ao receber `ESPNOW_MSG_RESTART` endereçado a ele, reinicia; se
  endereçado a um client que ele repassa, repassa via unicast/broadcast
  (mesma lógica de §12 gateway→client).

> Nota: hoje não há mensagem de restart no ar; `ESPNOW_MSG_COMMAND` só cobre
> ON/OFF. Adicionar o tipo e atualizar protocolo + clients juntos (regra 17).

## 10. Time Sync

Gateway → Client (`ESPNOW_MSG_TIME_SYNC`, broadcast):
```c
espnow_time_sync_t {
    uint8_t  msg_type;   // 0x08
    uint16_t sequence;
    uint8_t  gateway_mac[6];
    uint32_t epoch_seconds;
}
```
Enviado a cada `TIME_SYNC_INTERVAL_MS = 300000`.

## 11. Repeater (regra 19 do AGENTS.md)

- **Bidirecional**: gateway ↔ clients em ambas as direções.
- Repeater envia `ESPNOW_MSG_GW_DISCOVER` (broadcast) → gateway registra slot
  como `SENSOR_TYPE_REPEATER` e responde `ESPNOW_MSG_GW_ANNOUNCE`.
- Repeater envia `ESPNOW_MSG_REPEATER_STATUS` com
  `payload_repeater_status_t` (received, forwarded, client_count, channel,
  uptime_s, free_heap, ack_failures).
- Clients em repeater devem estar no **mesmo canal** do AP do gateway; canal
  diferente quebra ESP-NOW.

## 12. Gateway ↔ Repetidor (Encaminhamento)

O repetador (`clients/esp8266_repeater`) estende o alcance do ESP-NOW e é
**bidirecional** (regra 19): encaminha tráfego nos dois sentidos.

### Descoberta
1. Repeater envia `ESPNOW_MSG_GW_DISCOVER` (broadcast) no boot/período.
2. Gateway registra slot como `SENSOR_TYPE_REPEATER` e responde
   `ESPNOW_MSG_GW_ANNOUNCE`.
3. Repeater aprende o MAC do gateway pela source do `GW_ANNOUNCE` (MAC alt,
   regra 19) e o adiciona como peer.

### Encaminhamento (RX callback do repeater)
- **Client → Gateway**: repeater recebe frame de client (SENSOR_DATA,
  HEARTBEAT, PAIR_REQUEST, etc), faz `cache_sequence(seq, mac)` e reenvia
  **via broadcast** para o gateway (`fwd_push(s_bcast_addr, ...)`).
  Unicast ESP8266→ESP32 é dropado (regra 18), por isso obrigatoriamente
  broadcast.
- **Gateway → Client**: repeater recebe frame do gateway (COMMAND, ACK,
  PAIR_RESPONSE, TIME_SYNC, etc). Se reconhece a `sequence` origada por um
  client conhecido (`lookup_sequence`), encaminha **unicast** para esse client;
  senão faz broadcast para todos os clients.
- Frames são **deferidos para `loop()`** (`process_forward_queue()`):
  `esp_now_send()` não deve ser chamado dentro do RX callback. Contador
  `s_forwarded` alimenta `payload_repeater_status_t.forwarded`.

### Aprendizado de peers
- Clients desconhecidos são adicionados como peers (`s_client_peers`,
  máx. `MAX_PEERS`) ao aparecerem.
- Peers (gateway e broadcast) são **re-adicionados periodicamente** quando o
  canal WiFi muda: peers ESP-NOW são fixados ao canal; canal stale faz o
  gateway (ESP32) nunca receber o frame encaminhado.

### Reachability (feedback de uplink)
- Repeater considera o uplink OK quando recebe `ESPNOW_MSG_ACK` do gateway
  endereçado ao seu MAC (`ack->sensor_mac == my_mac`). ACKs de outros clients
  (ex: luzes) são ignorados para não dar falso positivo.

## 13. Clientes ↔ Clientes

No protocolo atual **não existe rota direta cliente↔cliente via gateway**.
Toda comunicação é always relayed através do gateway (ou do repeater):

- Cliente A nunca endereça cliente B diretamente. Um client só fala com o
  gateway (broadcast) e escuta respostas do gateway (broadcast).
- Se cliente A precisa influenciar cliente B (ex: sensor de presença acende
  luz), o fluxo é:
  `A →(broadcast) Gateway →(COMMAND broadcast) B`.
- O repeater só repassa; não roteia cliente→cliente por conta própria (ele
  encaminha client→gateway e gateway→client, nunca client→client).
- **Consequência**: para um cliente atuar sobre outro, o gateway deve conhecer
  ambos (pareados) e haver lógica no bridge/gateway para disparar o COMMAND.
  Não há multicast group nem endereçamento client-to-client no ar.

> Decisão de design: manter topologia estrela (todos os clients falam com o
> gateway). Cliente↔cliente direto só é possível se ambos estiverem no alcance
> rádio E no mesmo canal — mas o protocolo não define mensagens peer-to-peer,
> então NÃO é suportado oficialmente.

## 13.1 Papel do Repeater em ACK/NAK (regra de não-resposta)

O repetidor é um **mero repassador** e **não é endpoint** das mensagens que
repassa. Portanto:

- **O repeater NÃO deve enviar `ESPNOW_MSG_ACK` para pacotes que apenas
  encaminha** (client→gateway ou gateway→client). O ACK é responsabilidade
  exclusiva do endpoint real (gateway para SENSOR_DATA/HEARTBEAT/REPEATER_STATUS;
  client para COMMAND).
- O repeater **só pode emitir feedback negativo (NAK / status de falha) quando
  ele próprio não consegue falar com o gateway** (uplink quebrado). Nesse caso
  ele sinaliza a condição em seu `payload_repeater_status_t` (ex:
  `ack_failures`, ou um campo de status de uplink), e não inventa ACK em nome
  do gateway.
- Motivo: clients (ex: luzes) também fazem broadcast de ACK. Se o repeater
  respondesse ACK pelos frames repassados, o client destinatário receberia um
  falso ACK e o gateway poderia nunca ter recebido o dado. Isso corrompe a
  confirmação de entrega fim-a-fim.
- O ACK do gateway endereçado ao MAC do repeater é usado **apenas como prova
  de uplink** (`s_last_gateway_ack`), não como ACK de cliente.

> Estado atual do `clients/esp8266_repeater`: correto — não há `esp_now_send`
> de ACK próprio; o repeater só observa/encaminha. Manter assim.

## 13.2 Pareamento via Repeater — fluxo autorizado

Quando um client se pareia **através do repeater** (client → repeater →
gateway), o repeater é o intermediário e **não é endpoint** do pareamento.
Fluxo obrigatório:

1. **Client** envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast).
2. **Repeater** recebe. Se **não tiver gateway configurado/uplink OK**,
   responde **NAK** ao client imediatamente (não repassa inútil). Esse NAK é o
   único ACK/NAK que o repeater emite por conta própria (§13.1: só quando não
   consegue falar com o gateway).
3. Se tiver gateway, o repeater **repassa** o `PAIR_REQUEST` ao gateway
   (broadcast, regra 18) e **aguarda** a resposta.
4. **Gateway** processa segundo sua política (modo pareamento, slot livre,
   etc) e responde `ESPNOW_MSG_PAIR_RESPONSE` com status
   (`PAIR_STATUS_OK` / `PAIR_STATUS_FULL` / `PAIR_STATUS_DENIED`) — esse é o
   ACK/NAK autoritativo do pareamento.
5. **Repeater** recebe a `PAIR_RESPONSE` do gateway e a **repassa ao client**
   (unicast se conna o peer/client_chip, senão broadcast). O client confirma
   o pareamento com base nessa resposta, não com base no repeater.
6. Sem `PAIR_RESPONSE` do gateway (uplink quebrado ou timeout), o repeater
   **não confirma** o pareamento e sinaliza falha de uplink; o client faz
   retry no `loop()` (regra 15).

Regra de ouro: **um client só está pareado se o repeater confirmou com o
gateway**. Sem a `PAIR_RESPONSE` com `PAIR_STATUS_OK` repassada do gateway ao
client (fim-a-fim), o client **não está pareado** — o repeater não pode
declarar pareamento por conta própria. O ACK ou NAK final sempre vem da
política do gateway; o repeater só emite NAK próprio quando não tem gateway
para repassar.

> Estado atual do `clients/esp8266_repeater`: o `PAIR_REQUEST`/`PAIR_RESPONSE`
> só extrai a `sequence` no RX e o repeater usa `GW_DISCOVER` para se
> registrar (não entra em pareamento como client). Para adotar este fluxo, o
> repeater precisa: (a) responder NAK se `!s_gateway_configured`; (b) aguardar
> e repassar a `PAIR_RESPONSE` ao client originário (usando `cache_sequence`/
> `lookup_sequence` já existentes).

## 13.3 Queda do Gateway — notificação de re-pareamento

Se o gateway parar de responder (uplink repeater→gateway perdido), os clients
que parearam **via aquele repeater** ficam órfãos: o slot no gateway deixou de
ser alcançável/confirmável. O repeater deve agir:

- O repeater já detecta gateway ausente por timeout de comunicação
  (`s_last_gateway_comm`, hoje 60s) e limpa `s_gateway_configured`.
- Ao detectar a queda, o repeater **deve notificar os clients conhecidos**
  (`s_client_peers`) que é necessário **novo pareamento** — pois, pela §13.2,
  nenhum client está pareado sem confirmação do gateway.
- Mecanismo proposto: enviar um frame de **NAK de pareamento** (ou um
  `ESPNOW_MSG_GW_DISCOVER`/anúncio de "gateway perdido") em broadcast e/ou
  unicast para cada `s_client_peers`. O client, ao receber, marca-se
  `unpaired` e reinicia sua máquina de pareamento (retry no `loop()`, regra 15).
- Enquanto o gateway não for redescoberto (`GW_DISCOVER` → `GW_ANNOUNCE`), o
  repeater **não deve aceitar nem confirmar** `PAIR_REQUEST` de clients (responde
  NAK, §13.2 passo 2).
- Após o gateway voltar (novo `GW_ANNOUNCE`), o repeater retoma o repasse
  normal e os clients re-pareiam através dele.

Regra: **client só está pareado enquanto o gateway for alcançável pelo
repeater**. Queda do gateway ⇒ notificar re-pareamento; volta do gateway ⇒
liberar novo pareamento.

## 14. Decisão de Envio Unicast/Broadcast por Cliente (requisito)

Como a §2 estabelece, o modo de envio (unicast vs broadcast) depende do
**par (tx_chip, rx_chip)** e deve ser decidido **antes** do `esp_now_send`.
Portanto **gateway e repeater precisam saber o chip de cada client de destino**.

### Problema atual
- `virtual_sensor_t` (registry) **não tem** campo de chip do client.
- `espnow_pair_request_t` **não traz** o chip do client.
- Gateway e repeater hoje enviam **sempre broadcast** (`s_bcast_addr`), então
  não usam unicast nem para ESP8266→ESP8266, perdendo a eficiência e
  aumentando colisões.

### Solução proposta (protocolo)
1. Adicionar `uint8_t client_chip` a `espnow_pair_request_t`
   (0 = ESP8266, 1 = ESP32, 0xFF = desconhecido), informado pelo client no
   pareamento. `espnow_protocol.h` e todos os clients devem ser atualizados
   juntos (regra 17 de tamanho/compatibilidade de estrutura).
2. Gateway armazena `client_chip` em `virtual_sensor_t` e persiste em NVS.
3. Helper de envio decide o destino:
   ```c
   // tx = gateway(ESP32) ou repeater(ESP8266); rx_chip = s->client_chip
   const uint8_t *dest = (rx_chip == CHIP_ESP8266) ? s->mac : s_bcast_addr;
   esp_now_send(dest, data, len);
   ```
   - Se rx é ESP8266 → **unicast OK** (ESP32→ESP8266 e ESP8266→ESP8266).
   - Se rx é ESP32 → **broadcast obrigatório** (só ocorre repeater ESP8266→
     gateway ESP32, coberto pela regra da §2).
   - Se desconhecido → broadcast (fallback seguro).
4. Repeater: ao encaminhar **gateway→client**, usa unicast se conhece o
   `client_chip` do peer; senão broadcast. Ao encaminhar **client→gateway**,
   mantém broadcast (client ESP8266 → gateway ESP32, regra 18).

### Peer channel
Antes de qualquer unicast, o peer de destino deve estar adicionado no canal
correto (`espnow_add_peer_wrapper` / `esp_now_add_peer`). Peers são fixados ao
canal; re-adicionar se o WiFi mudar de canal (ver §12).

## 16. Operação Autônoma (sem roteador / infraestrutura)

Um client deve conseguir **operar mesmo sem roteador WiFi de infraestrutura**,
dando preferência ao ESP-NOW como transporte primário:

- O client roda ESP-NOW em `WIFI_AP_STA` (ou `WIFI_STA` sem `WiFi.begin()`),
  então o rádio ESP-NOW funciona **independentemente de haver AP associado**.
  Comprovado em `test_espnow` (ambos `WiFi.disconnect()` + COMBO + canal
  explícito funcionou).
- **Detecção de ausência de roteador**: se `WiFi.status() != WL_CONNECTED`
  após o timeout de associação STA, o client entra em **modo autônomo**:
  - Mantém ESP-NOW ativo e continua pareando/enviando dados via gateway ou
    repeater (que também são STA no mesmo canal).
  - Sobe **AP de config local** (`softAP`) para o usuário poder configurar
    WiFi/credenciais (já feito em `hwifi_begin()` dos clients lampada/onoff),
    mas o ESP-NOW segue operando normalmente (não bloqueia o rádio).
  - Não deve travar nem fazer `delay()` longo esperando o STA; usa máquina de
    estados (regra 15).
- **Preferência ESP-NOW**: o transporte de dados/controle é sempre ESP-NOW
  (não HTTP direto, regra 12 dos AI rules). O WiFi STA serve apenas para
  bridge/MQTT/OTA pela rede — se ausente, o client continua funcional no ar.
- **Canal**: como o client pode não estar associado a AP, ele deve usar o
  canal explícito do gateway/repeater (`ESP_NOW_CHANNEL`) e não derivar o
  canal do STA. Se estiver em STA+AP sem AP, fixar o canal do ESP-NOW
  explicitamente para não ficar em canal 0/indefinido.
- **Repeater como ponte**: em ambiente sem roteador, o repeater (que também é
  autônomo) é a única ponte para o gateway; o client deve priorizar parear/
  falar via repeater se não houver gateway direto (ver §12/§13.2).
- **OTA**: sem roteador, OTA pela rede não é possível; manter OTA via AP local
  ou via gateway/repeater quando houver link.

Regra: **ESP-NOW é o transporte canônico e deve operar com ou sem roteador;
ausência de infraestrutura WiFi não deve degradar o funcionamento do client.**

## 17. Regras de Implementação (checklist)

- [ ] Loop non-blocking (sem `delay()`): máquina de estados com `millis()`
      (regra 15).
- [ ] Client → Gateway sempre BROADCAST (regra 18).
- [ ] `device_name`/name com 32 bytes; atualizar gateway+clients juntos (regra 17).
- [ ] Validar `version` e tamanho mínimo no RX; contar `crc_errors`.
- [ ] Persistir devices em NVS; restaurar no boot.
- [ ] Qualquer mudança de estado no client → feedback imediato ao gateway
      (regra 14).
- [ ] Repeater obrigatoriamente bidirecional (regra 19).
- [ ] Mesmo canal do AP para todos (clients, repeaters, gateway).
- [ ] `FW_VERSION` idêntico entre gateway, bridge e clients (regra 13).
