# TCP Radio Interface — Design Spec

**Data:** 2026-07-29  
**Status:** Draft  
**Autor:** Orchestrator + User  


** avaliar usar uma api webserver, ex: /cmd/x
UDP, somente se não tiver um servidor configurado no via dashboard ou portal

---

## 1. Visão Geral

Implementar uma nova interface de rádio TCP (`TcpRadio`) que permite comunicação entre nodes e hub via TCP/IP, habilitando nodes em LANs diferentes do hub. O TCP é tratado como mais um tipo de rádio dentro da abstração `RadioInterface` já existente.

### 1.1 Cenário Principal
- Node conectado a WiFi em LAN **diferente** do hub
- Comunicação via TCP/IP roteável (não depende de layer 2)
- Discovery automático via UDP broadcast (porta 5000) ou um servidor configurado pelo dashboard do node;

### 1.2 Motivação
- ESP-NOW: limitado a mesma LAN/canal WiFi (layer 2)
- TCP: qualquer IP roteável, throughput maior, confiabilidade nativa
- Complementa ESP-NOW: node com WiFi usa TCP; node sem WiFi usa ESP-NOW

---

## 2. Arquitetura

### 2.1 Abstração Existente (não modificar)

```
RadioInterface (shared/src/radio_interface.h)
├── EspnowHandler (hub/src/espnow_handler.cpp)   → RADIO_ESPNOW = 0
├── LoraHandler   (hub/src/lora_handler.cpp)      → RADIO_LORA = 1
└── TcpRadio      (hub/src/tcp_handler.cpp)       → RADIO_TCP = 2  ← NOVO
```

- `RadioManager` gerencia múltiplos radios (max 4)
- `sensor_registry` já rastreia `radio_type` por node
- `device_router` roteia comandos via `RadioManager`

### 2.2 Novos Arquivos

| Arquivo | Local | Responsabilidade |
|---------|-------|------------------|
| `tcp_protocol.h` | `shared/src/` | Definições de msg types, structs, constants |
| `tcp_handler.h/.cpp` | `hub/src/` | TcpRadio (server side) — implementa RadioInterface |
| `tcp_node_protocol.h/.cpp` | `shared/src/` | TcpNodeProtocol (client side) — implementa NodeProtocol |

### 2.3 Flags de Compilação

```cpp
// hub platformio.ini
build_flags = -DHABILITA_TCP

// node platformio.ini (nodes TCP)
build_flags = -DHABILITA_TCP-radio
```

`HABILITA_TCP` ativa o código TCP no hub. `HABILITA_TCP-radio` ativa o node protocol TCP.

---

## 3. Protocolo TCP

### 3.1 Formato de Mensagem

Mensagens são frames binários com header fixo:

```
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ msg_type (1) │ sensor_id(6) │ sensor_type(1)│ payload_len(1)│ payload(N)  │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
Total header: 9 bytes
```

**Campos:**
- `msg_type` (1 byte): tipo da mensagem (ver enum abaixo)
- `sensor_id` (6 bytes): MAC do node (identificador único)
- `sensor_type` (1 byte): tipo do sensor (sensor_type_t)
- `payload_len` (1 byte): tamanho do payload (0-240)
- `payload` (N bytes): dados específicos do msg_type

### 3.2 Tipos de Mensagem

```cpp
enum tcp_msg_type_t {
    TCP_MSG_SENSOR_DATA   = 0x01,
    TCP_MSG_PAIR_REQUEST  = 0x02,
    TCP_MSG_PAIR_RESPONSE = 0x03,
    TCP_MSG_HEARTBEAT     = 0x05,
    TCP_MSG_COMMAND       = 0x07,
    TCP_MSG_TIME_SYNC     = 0x08,
    TCP_MSG_GW_ANNOUNCE   = 0x09,
    TCP_MSG_GW_DISCOVER   = 0x0A,
    TCP_MSG_RESTART       = 0x0C,
};
```

### 3.3 Diferenças do Protocolo ESP-NOW

| Campo | ESP-NOW | TCP |
|-------|---------|-----|
| `sequence` | Sim (uint16_t) | **Não** (TCP garante ordem) |
| `rssi` | Sim (int16_t) | **Não** (não aplicável) |
| `battery_pct` | No header | No payload (se necessário) |
| `version` | Sim | **Não** (protocolo novo) |
| ACK/NAK | App-level | **Não** (confia no TCP) |
| Retry | App-level (3x) | **Não** (confia no TCP) |

### 3.4 Payloads por Msg Type

**TCP_MSG_SENSOR_DATA (0x01):**
- Payload: sensor específico (payload_temp_hum_t, payload_dht_gas_t, etc.)
- Reusa os mesmos structs de `espnow_protocol.h`

**TCP_MSG_PAIR_REQUEST (0x02):**
```
payload:
  uint8_t  sensor_type
  uint8_t  firmware_version[4]
  char     device_name[32]
  uint8_t  client_chip
```

**TCP_MSG_PAIR_RESPONSE (0x03):**
```
payload:
  uint8_t  status        // PAIR_STATUS_OK / FULL / DENIED
  uint16_t assigned_slot
  uint8_t  gateway_mac[6]
```

**TCP_MSG_HEARTBEAT (0x05):**
- Payload: vazio (só header)

**TCP_MSG_COMMAND (0x07):**
```
payload:
  uint8_t command
  char    target_device_id[32]
```

**TCP_MSG_TIME_SYNC (0x08):**
```
payload:
  uint8_t  gateway_mac[6]
  uint32_t epoch_seconds
```

**TCP_MSG_GW_DISCOVER (0x0A):**
- Payload: vazio (node descobre hub)

**TCP_MSG_GW_ANNOUNCE (0x09):**
```
payload:
  uint8_t gateway_mac[6]
  uint8_t fw_version[4]
```

---

## 4. Discovery e Conexão

### 4.1 Fluxo de Discovery

```
Node (LAN B)                          Hub (LAN A)
     │                                      │
     │──── UDP Broadcast :5000 ────────────>│  GW_DISCOVER (0x0A)
     │     (FF:FF:FF:FF:FF:FF)              │
     │                                      │
     │<─── UDP Unicast :5000 ──────────────│  GW_ANNOUNCE (0x09)
     │     (hub IP + MAC + FW)              │
     │                                      │
     │──── TCP Connect :5000 ─────────────>│  (estabelece socket)
     │                                      │
     │──── TCP PAIR_REQUEST ──────────────>│  (sensor_type + name)
     │                                      │
     │<─── TCP PAIR_RESPONSE ─────────────│  (slot + status)
     │                                      │
     │════ TCP Connection Open ════════════│  (dados contínuos)
```

### 4.2 Discovery UDP (reutiliza mecanismo existente)

- Node envia `GW_DISCOVER` (1 byte: `0x0A`) via UDP broadcast porta 5000
- Hub responde com `GW_ANNOUNCE` via UDP unicast
- Mesmo formato do discovery atual (`scan.py` já faz isso)

### 4.3 Conexão TCP

- Node conecta ao IP do hub na porta 5000
- Socket TCP permanente (mantido aberta)
- Se desconexão: node tenta reconexão a cada 5s (máx 20 tentativas)
- Hub aceita múltiplas conexões TCP (uma por node)

### 4.4 Porta TCP

**Usa porta 5000** (mesma do UDP discovery). O hub escuta:
- UDP :5000 — discovery (broadcast)
- TCP :5000 — dados dos nodes (socket connections)

---

## 5. Hub — TcpRadio

### 5.1 Implementação

```cpp
// hub/src/tcp_handler.h
class TcpRadio : public RadioInterface {
public:
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

    // RadioInterface overrides
    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;
    bool start_pairing() override;
    void announce() override;
    void broadcast_time_sync(uint32_t epoch) override;

    unsigned long get_rx_count() const override;
    unsigned long get_ack_count() const override;

private:
    WiFiServer m_server;
    WiFiClient m_clients[MAX_TCP_CLIENTS];  // max 4
    int m_client_count = 0;
    unsigned long m_rx_count = 0;
    unsigned long m_tx_count = 0;

    void handle_new_client();
    void handle_client_data(int client_idx);
    void process_frame(int client_idx, const uint8_t* data, size_t len);
    void send_to_client(int client_idx, const uint8_t* data, size_t len);
    int find_client_by_mac(const uint8_t* mac);
};
```

### 5.2 Constantes

```cpp
#define TCP_SERVER_PORT 5000
#define TCP_MAX_CLIENTS 4
#define TCP_BUFFER_SIZE 256
#define TCP_FRAME_MAX 250
```

### 5.3 Loop

```cpp
void TcpRadio::loop() {
    handle_new_client();        // aceita novas conexões

    for (int i = 0; i < m_client_count; i++) {
        if (m_clients[i].connected()) {
            handle_client_data(i);  // lê dados do client
        } else {
            remove_client(i);       // remove client desconectado
        }
    }
}
```

### 5.4 Envio

```cpp
int TcpRadio::send(const uint8_t* data, size_t len) {
    // Envia para TODOS os clients conectados (broadcast)
    for (int i = 0; i < m_client_count; i++) {
        if (m_clients[i].connected()) {
            m_clients[i].write(data, len);
            m_tx_count++;
        }
    }
    return 0;
}
```

### 5.5 Recebimento

```cpp
void TcpRadio::handle_client_data(int client_idx) {
    uint8_t buf[TCP_BUFFER_SIZE];
    int n = m_clients[client_idx].read(buf, sizeof(buf));
    if (n > 0) {
        m_rx_count++;
        process_frame(client_idx, buf, n);
    }
}
```

---

## 6. Node — TcpNodeProtocol

### 6.1 Implementação

```cpp
// shared/src/tcp_node_protocol.h
class TcpNodeProtocol : public NodeProtocol {
public:
    void begin() override;
    void loop() override;
    bool is_paired() const override;
    uint8_t assigned_slot() const override;
    void force_repair() override;

    void set_hub_ip(IPAddress ip);
    void set_device_name(const char* name);

private:
    WiFiClient m_client;
    IPAddress m_hub_ip;
    bool m_paired;
    uint8_t m_slot;
    char m_device_name[32];
    uint8_t m_sensor_type;
    unsigned long m_last_heartbeat_ms;
    unsigned long m_last_state_ms;
    unsigned long m_last_pair_ms;
    int m_pair_attempts;
    bool m_connected;

    void connect_to_hub();
    void send_pair_request();
    void send_sensor_data();
    void send_heartbeat();
    void handle_response(const uint8_t* data, size_t len);
    void discover_hub();  // UDP broadcast
};
```

### 6.2 Loop

```cpp
void TcpNodeProtocol::loop() {
    if (!m_connected) {
        discover_hub();      // tenta descobrir hub via UDP
        connect_to_hub();    // tenta conectar TCP
        return;
    }

    // Lê respostas do hub
    if (m_client.available()) {
        uint8_t buf[TCP_BUFFER_SIZE];
        int n = m_client.read(buf, sizeof(buf));
        if (n > 0) handle_response(buf, n);
    }

    if (!m_paired) {
        // Envia PAIR_REQUEST periodicamente
        if (millis() - m_last_pair_ms > 5000) {
            m_last_pair_ms = millis();
            send_pair_request();
        }
        return;
    }

    // Envia dados do sensor periodicamente
    if (millis() - m_last_state_ms >= STATE_UPDATE_INTERVAL) {
        m_last_state_ms = millis();
        send_sensor_data();
    }

    // Heartbeat
    if (millis() - m_last_heartbeat_ms >= 60000) {
        m_last_heartbeat_ms = millis();
        send_heartbeat();
    }
}
```

---

## 7. Integração com RadioManager

### 7.1 Registro no Hub

```cpp
// hub/src/main.cpp
#ifdef HABILITA_TCP
#include "tcp_handler.h"
static TcpRadio s_tcp;
#endif

void setup() {
    // ...
    #ifdef HABILITA_ESPNOW
    s_radio_mgr.add_radio(RADIO_ESPNOW, &s_espnow);
    #endif
    #ifdef HABILITA_LORA
    s_radio_mgr.add_radio(RADIO_LORA, &s_lora);
    #endif
    #ifdef HABILITA_TCP
    s_radio_mgr.add_radio(RADIO_TCP, &s_tcp);
    #endif
    s_radio_mgr.init_all();
}
```

### 7.2 Enum radio_type_t

```cpp
// hub/include/sensor_registry.h
enum radio_type_t {
    RADIO_ESPNOW = 0,
    RADIO_LORA   = 1,
    RADIO_TCP    = 2,
};
```

### 7.3 Sensor Registry

Sem alterações necessárias — `sensor_registry_add()` já aceita `radio_type` como parâmetro. O TCP handler passará `RADIO_TCP` ao registrar nodes.

---

## 8. Reutilização de Código

### 8.1 Reutiliza de espnow_protocol.h
- `sensor_type_t` (enums de tipo de sensor)
- Payload structs (`payload_temp_hum_t`, `payload_dht_gas_t`, etc.)
- `mac_to_str()`, `mac_equal()`, `mac_copy()`
- `PAIR_STATUS_OK`, `PAIR_STATUS_FULL`, `PAIR_STATUS_DENIED`

### 8.2 Reutiliza de node_protocol.h
- `NodeCallbacks` (get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart)
- `NodeProtocol` (interfaz base)

### 8.3 Reutiliza de common_espnow.h
- `espnow_load_device_name()`, `espnow_save_device_name()`
- EEPROM functions

### 8.4 NÃO reutiliza
- `espnow_header_t` (tem campos desnecessários para TCP: sequence, rssi, version)
- `espnow_send_wrapper()` (usa `esp_now_send()`)
- `espnow_client_init()` (usa `esp_now_init()`)
- ACK/retry logic

---

## 9. Constraints e Limitações

### 9.1 Memória
- TCPClient consome ~1-2KB de RAM por conexão
- Max 4 clients simultâneos = ~4-8KB extra no hub
- ESP8266: ~40KB livres → viável com 4 clients
- ESP32: ~200KB livres → tranquilo

### 9.2 Throughput
- TCP/IP overhead: ~40 bytes por packet
- Sensor data: ~10-20 bytes de payload
- Intervalo mínimo recomendado: 5s (vs 60s em ESP-NOW)
- Throughput efetivo: ~10 Mbps (suficiente para qualquer sensor)

### 9.3 Latência
- TCP handshake: ~1-2ms na LAN, ~10-50ms cross-LAN
- Dados: ~1-5ms ( LAN), ~10-100ms (cross-LAN)
- Acceptable para sensores (update interval ≥ 5s)

### 9.4 Energia
- WiFi ativo consome mais que ESP-NOW
- Não recomendado para nodes com bateria
- TCP é para nodes com alimentação fixa

### 9.5 Segurança
- TCP plaintext (sem TLS) — aceitável para rede local
- Se necessário no futuro: adicionar TLS (mbedTLS)
- Discovery UDP é broadcast (qualquer um pode ver)

---

## 10. Casos de Uso

### 10.1 Node sensor em prédio diferente
- Node DHT22 em prédio separado, conectado a roteador próprio
- Hub no prédio principal
- TCP cross-LAN permite comunicação sem cabos extras

### 10.2 Node em rede de convidado
- Node em VLAN de convidado, hub na rede principal
- TCP via roteamento entre VLANs

### 10.3 Hub cloud (futuro)
- Hub em servidor cloud, nodes em campo
- TCP nativo funciona sem ESP-NOW (que é local)
- Descoberta via DNS fixo em vez de UDP broadcast

---

## 11. Riscos e Mitigações

| Risco | Impacto | Mitigação |
|-------|---------|-----------|
| Hub unreachable | Node não consegue enviar dados | Retry com backoff; reconexão automática |
| Muitos clients TCP | RAM insuficiente | Limitar a 4 clients; rejeitar novas conexões |
| UDP broadcast bloqueado | Discovery falha | Fallback: IP fixo configurável via WiFiManager |
| Firewall bloqueia TCP | Comunicação impossível | Documentar portas necessárias (5000 TCP/UDP) |
| Latência alta cross-LAN | Heartbeat timeout falso | Aumentar SENSOR_TIMEOUT_MS para nodes TCP |

---

## 12. Testing

### 12.1 Unit Tests
- Parser de frame TCP (deserialize msg_type, sensor_id, payload)
- Serializer de frame TCP (serialize para envio)
- State machine de conexão (connect, reconnect, timeout)

### 12.2 Integration Tests
- Hub + 1 node TCP na mesma LAN
- Hub + 1 node TCP em LAN diferente (router entre)
- Hub + múltiplos nodes TCP simultâneos
- Fallback ESP-NOW quando TCP indisponível

### 12.3 Manual Tests
- Flash hub com HABILITA_TCP
- Flash node com HABILITA_TCP-radio
- Verificar discovery UDP
- Verificar pairing via TCP
- Verificar envio de dados do sensor
- Verificar comando remoto (toggle relay)
- Verificar reconexão após desconexão

---

## 13. Checklist de Implementação

### Hub
- [ ] `shared/src/tcp_protocol.h` — definições de protocolo
- [ ] `hub/src/tcp_handler.h` — classe TcpRadio
- [ ] `hub/src/tcp_handler.cpp` — implementação
- [ ] `hub/include/sensor_registry.h` — adicionar `RADIO_TCP = 2`
- [ ] `hub/src/main.cpp` — registrar TcpRadio no RadioManager
- [ ] `hub/include/config.h` — adicionar `HABILITA_TCP` constants
- [ ] `hub/platformio.ini` — build_flags com `-DHABILITA_TCP`

### Node (exemplo: climate-gas)
- [ ] `shared/src/tcp_node_protocol.h` — classe TcpNodeProtocol
- [ ] `shared/src/tcp_node_protocol.cpp` — implementação
- [ ] `nodes/climate-gas/src/main.cpp` — usar TcpNodeProtocol quando `HABILITA_TCP-radio`
- [ ] `nodes/climate-gas/platformio.ini` — build_flags com `-DHABILITA_TCP-radio`

### Shared
- [ ] `shared/src/tcp_protocol.h` — struct definitions, msg types
- [ ] Atualizar `shared/src/node_protocol.h` se necessário

### Testes
- [ ] Teste discovery UDP hub→node
- [ ] Teste pairing TCP
- [ ] Teste envio/recebimento dados
- [ ] Teste reconexão
- [ ] Teste múltiplos nodes

---

## 14. Futuras Extensões

- **TLS/SSL**: adicionar mbedTLS para comunicação segura
- **Autenticação**: token/chave no PAIR_REQUEST
- **Compressão**: gzip payloads para throughput maior
- **Keepalive**: TCP keepalive configurável
- **Fallback automático**: se TCP falhar, tenta ESP-NOW (se disponível)
