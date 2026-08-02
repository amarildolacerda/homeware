# Design: TCP Radio — HTTP REST + UDP Discovery

**Data:** 2026-08-01
**Status:** Aprovado (combinado com UDP discovery)
**Base:** 2026-07-29-tcp-radio-design.md (TCP raw) + 2026-08-01 (HTTP REST)

## 1. Visão Geral da Arquitetura

```
┌─────────────┐     UDP Broadcast   ┌─────────────┐
│  TCP Node   │ ──────────────────▶ │   Hub       │
│  (ESP8266/  │ ◀────────────────── │ (ESP32)     │
│   ESP32)    │     UDP Unicast     │             │
└─────────────┘                     └─────────────┘
       │                                  │
       │     HTTP POST (dados)            │
       │ ────────────────────────────────▶│
       │     HTTP GET (comandos)          │
       │ ◀────────────────────────────────│
       │                                  │
       │ WiFi (LAN)                       │ MQTT
       │                                  ▼
       │                           ┌─────────────┐
       │                           │ Home Assist. │
       └──────────────────────────▶│ (Bridge)    │
              (dashboard)          └─────────────┘
```

- **Node TCP**: ESP com WiFi que envia dados via HTTP POST para o hub
- **Hub**: ESP32 que recebe dados TCP e alimenta `sensor_registry` → MQTT → HA
- **Discovery**: UDP broadcast porta 5000 (mesmo mecanismo do bridge)
- **Transporte**: HTTP REST sobre WiFi LAN (sem autenticação)
- **Bidirecional**: Node POSTa dados, hub responde com comandos pendentes via GET
- **Caso de uso**: Nodes em locais com WiFi mas sem alcance ESP-NOW/LoRa

## 2. Discovery UDP

### 2.1 Lógica de Descoberta

O node usa UDP discovery **apenas quando necessário**:

| Condição | Comportamento |
|----------|---------------|
| Hub IP **não configurado** (primeiro boot) | Envia UDP GW_DISCOVER até encontrar hub |
| Hub IP **configurado** (EEPROM) | Usa IP salvo, **sem UDP** |
| Hub configurado mas **não responde** (timeout) | Fallback: envia UDP GW_DISCOVER para redescobrir |
| Hub IP fixo via WiFiManager | Usa IP fixo, sem UDP |

**Prioridade:** IP configurado > UDP discovery > IP fixo (HUB_IP_DEFAULT)

### 2.2 Fluxo (primeiro boot / sem IP configurado)

```
Node (LAN B)                          Hub (LAN A)
     │                                      │
     │──── UDP Broadcast :5000 ────────────▶│  GW_DISCOVER
     │     (FF:FF:FF:FF:FF:FF)              │
     │                                      │
     │◀─── UDP Unicast :5000 ──────────────│  GW_ANNOUNCE
     │     (hub IP + port)                  │
     │                                      │
     │──── HTTP POST /node/register ───────▶│  (registra node)
     │                                      │
     │◀─── HTTP 200 OK ───────────────────│  (slot atribuído)
     │                                      │
     │════ Conexão HTTP persistente ════════│  (dados contínuos)
```

### 2.3 Fluxo (IP já configurado)

```
Node (LAN B)                          Hub (LAN A)
     │                                      │
     │──── HTTP POST /node/register ───────▶│  (usa IP do EEPROM)
     │                                      │
     │◀─── HTTP 200 OK ───────────────────│  (slot atribuído)
     │                                      │
     │════ Conexão HTTP persistente ════════│  (dados contínuos)
```

### 2.4 Fallback (hub caiu)

```
Node (LAN B)                          Hub (LAN A)
     │                                      │
     │──── HTTP POST /node/state ──────────▶│  TIMEOUT (hub offline)
     │                                      │
     │──── UDP Broadcast :5000 ────────────▶│  GW_DISCOVER (redescoberta)
     │                                      │
     │◀─── UDP Unicast :5000 ──────────────│  GW_ANNOUNCE (novo IP)
     │                                      │
     │──── HTTP POST /node/register ───────▶│  (re-registra com novo IP)
```

### 2.2 Formato das Mensagens UDP

**GW_DISCOVER (Node → Hub):**
```cpp
// Broadcast UDP porta 5000
struct __attribute__((packed)) {
    uint8_t msg_type;     // 0x0A (TCP_MSG_GW_DISCOVER)
    uint8_t sensor_type;  // tipo do sensor
    char device_name[32]; // nome do node
};
```

**GW_ANNOUNCE (Hub → Node):**
```cpp
// Unicast UDP porta 5000
struct __attribute__((packed)) {
    uint8_t msg_type;      // 0x09 (TCP_MSG_GW_ANNOUNCE)
    uint8_t fw_version[4]; // versão do firmware
    char hub_ip[16];       // IP do hub (para HTTP)
    uint16_t hub_port;     // porta HTTP (80)
};
```

### 2.3 Compatibilidade

- Reusa o mesmo mecanismo de discovery do bridge (porta 5000)
- `scan.py` já detecta dispositivos nesta porta
- Hub escuta UDP :5000 para discovery + HTTP :80 para dados

## 3. Protocolo HTTP

### 3.1 Endpoints no Hub (servidor)

| Método | Path | Descrição |
|--------|------|-----------|
| `POST` | `/node/register` | Node se registra (device_id, sensor_type, device_name) |
| `POST` | `/node/state` | Node envia dados do sensor |
| `POST` | `/node/heartbeat` | Node envia keep-alive |
| `GET` | `/node/command/{device_id}` | Node busca comandos pendentes |

### 3.2 Formato JSON

```json
// POST /node/register
{
  "device_id": "agri_123456",
  "sensor_type": 1,
  "device_name": "TempSensor",
  "fw_version": "0.0.30"
}

// POST /node/state
{
  "device_id": "agri_123456",
  "temperature": 25.5,
  "humidity": 60.2
}

// POST /node/heartbeat
{
  "device_id": "agri_123456"
}

// GET /node/command/agri_123456
// Resposta com comando pendente (TTL: 30s, descartado após expirar):
{
  "command": "on",
  "slot": 5
}
// Ou vazio se não há comandos:
{}
```

### 3.3 Diferenças do Protocolo TCP Raw (spec 2026-07-29)

| Aspecto | TCP Raw (2026-07-29) | HTTP REST (atual) |
|---------|---------------------|-------------------|
| Transporte | Socket TCP persistente | HTTP connections |
| Formato | Binário (header 9 bytes) | JSON |
| Discovery | UDP broadcast | UDP broadcast (mesmo) |
| Conexão | Mantida aberta | Polling periódico |
| Throughput | Maior | Menor (suficiente para sensores) |
| Complexidade | Maior | Menor |

## 4. Hub — TcpRadioHandler

### 4.1 Implementação

```cpp
class TcpRadioHandler : public RadioInterface {
public:
    int init() override;          // Registra rotas HTTP + UDP server
    int send(const uint8_t* data, size_t len) override;  // Não utilizado
    void loop() override;         // Processa UDP + HTTP + limpa comandos expirados
    bool is_ready() const override;
    bool send_command(const uint8_t* mac, uint8_t state) override;  // Enfileira comando
    bool send_restart(const uint8_t* mac) override;  // Enfileira restart
private:
    WiFiUDP m_udp;
    std::map<std::string, std::vector<PendingCommand>> m_pending_commands;
    
    void handle_udp_discover();  // Responde UDP com GW_ANNOUNCE
    void cleanup_expired_commands();
    int find_slot_by_device_id(const char* device_id);
};
```

### 4.2 Constantes

```cpp
#define TCP_UDP_PORT 5000
#define TCP_HTTP_PORT 80
#define TCP_COMMAND_TTL_MS 30000
#define TCP_MAX_PENDING_COMMANDS 10
```

### 4.3 Loop

```cpp
void TcpRadioHandler::loop() {
    handle_udp_discover();      // Processa discovery UDP
    cleanup_expired_commands(); // Limpa comandos expirados
}
```

### 4.4 Compilação Condicional

- Hub: `-DTCP_ENABLED` em `platformio.ini`
- `TcpRadioHandler` só é instanciado quando `TCP_ENABLED` está definido

## 5. Node TCP

### 5.1 Estrutura de Diretórios

```
nodes/tcp/
├── platformio.ini
├── include/
│   ├── config.h
│   └── pages.h
└── src/
    └── main.cpp
```

### 5.2 Comportamento

1. WiFi não-bloqueante (regra 26)
2. `setup()`: conectar WiFi, carregar hub IP da EEPROM
3. `loop()`: se hub IP configurado → tenta HTTP direto; senão → UDP discovery
4. `loop()`: enviar dados em intervalo configurável, buscar comandos em `/node/command`
5. Se HTTP falhar por timeout → fallback para UDP discovery (redescobrir hub)
6. Dashboard padrão (sidebar 180px, stats-header, polling 3s)
7. OTA via `/api/ota`
8. Console/telnet obrigatório

### 5.3 Config.h

```cpp
#define HUB_IP_DEFAULT "192.168.1.100"  // IP padrão (fallback se UDP falhar)
#define HUB_PORT 80
#define UDP_PORT 5000
#define STATE_UPDATE_INTERVAL 10000  // 10s
#define HEARTBEAT_INTERVAL 30000     // 30s
#define DISCOVERY_INTERVAL 10000     // 10s (retry se não encontrado)
#define MAX_DISCOVERY_RETRIES 20
#define HTTP_TIMEOUT_MS 5000         // 5s timeout HTTP
#define HUB_FALLBACK_RETRIES 3       // Tentativas antes de UDP fallback
```

### 5.4 Lógica de Conexão

```cpp
void loop() {
    // Estado: aguardando hub
    if (!m_hub_found) {
        // 1. Tentar IP configurado (EEPROM)
        if (m_hub_ip_configured) {
            if (try_connect_http(m_hub_ip)) {
                m_hub_found = true;
                // Prosseguir para registro
            } else {
                m_http_fallback_retries++;
                if (m_http_fallback_retries > HUB_FALLBACK_RETRIES) {
                    // 2. IP configurado não responde → UDP discovery
                    m_hub_ip_configured = false;
                    m_http_fallback_retries = 0;
                }
            }
        }
        
        // 3. UDP discovery (se IP não configurado ou fallback)
        if (!m_hub_ip_configured) {
            if (millis() - m_last_discovery > DISCOVERY_INTERVAL) {
                send_udp_discover();
                m_last_discovery = millis();
                m_discovery_retries++;
                
                if (m_discovery_retries > MAX_DISCOVERY_RETRIES) {
                    // 4. Fallback final: HUB_IP_DEFAULT
                    m_hub_ip = HUB_IP_DEFAULT;
                    m_hub_found = true;
                }
            }
            
            // Escuta resposta UDP
            if (m_udp.parsePacket()) {
                handle_udp_announce();
                m_hub_found = true;
                // Salvar IP descoberto na EEPROM
                save_hub_ip_to_eeprom(m_hub_ip);
            }
        }
        return;
    }
    
    // Fluxo HTTP normal (register, state, heartbeat, command)
    // Se HTTP falhar m vezes → m_hub_found = false (trigger UDP fallback)
}
```

### 5.5 EEPROM Layout

```cpp
//Offsets para hub IP
#define EEPROM_HUB_IP_OFFSET 96      // 16 bytes (xxx.xxx.xxx.xxx\0)
#define EEPROM_HUB_IP_VALID 112      // 1 byte (0x01 = válido)
```

### 5.6 Salvar/Carregar Hub IP

```cpp
void save_hub_ip_to_eeprom(const String& ip) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(EEPROM_HUB_IP_OFFSET, ip.c_str());
    EEPROM.write(EEPROM_HUB_IP_VALID, 0x01);
    EEPROM.commit();
}

String load_hub_ip_from_eeprom() {
    EEPROM.begin(EEPROM_SIZE);
    if (EEPROM.read(EEPROM_HUB_IP_VALID) != 0x01) {
        return "";  // Não configurado
    }
    char buf[16];
    EEPROM.get(EEPROM_HUB_IP_OFFSET, buf);
    return String(buf);
}
```

## 6. Pareamento

### 6.1 Fluxo

1. Node envia `POST /node/register` com `device_id`, `sensor_type`, `device_name`
2. Hub busca `device_id` no `sensor_registry`:
   - Se encontrado → responde `200 OK` com `assigned_slot`
   - Se não → aloca slot livre, adiciona ao registry, responde `200 OK`
3. Node armazena `assigned_slot` e começa a enviar dados
4. Pareamento é **automático** (sem modo de pareamento, como ESP-NOW Fase 1)

### 6.2 Resposta do Hub

```json
{
  "status": "ok",
  "assigned_slot": 5,
  "device_id": "agri_123456"
}
```

## 7. Tratamento de Erros

### 7.1 Timeouts

- Node: `SENSOR_TIMEOUT_MS = 300000` (5 min) sem dados → `online=false`
- Hub: limpa slot se `last_seen > SENSOR_TIMEOUT_MS`

### 7.2 Retry

- Node: retry com backoff exponencial (1s, 2s, 4s, máx 30s)
- Hub: fila de comandos com TTL (30s)

### 7.3 Validação

- Hub valida `device_id` (imprimíveis 0x20–0x7E, regra 20)
- Hub valida `sensor_type` (1–10)
- Node valida resposta HTTP antes de confirmar registro

### 7.4 Fallback UDP Discovery

| Condição | Ação |
|----------|------|
| IP configurado + HTTP OK | Usa IP configurado |
| IP configurado + HTTP timeout (3x) | Envia UDP GW_DISCOVER |
| UDP discovery encontra hub | Salva novo IP na EEPROM |
| UDP discovery falha (20x) | Usa HUB_IP_DEFAULT |
| Hub IP muda (DHCP) | Node redescobre via UDP fallback |

### 7.5 Detecção de Queda do Hub

```cpp
// No loop, após enviar state:
if (http_falhou && m_hub_ip_configured) {
    m_http_fallback_retries++;
    if (m_http_fallback_retries > HUB_FALLBACK_RETRIES) {
        // Hub caiu → trigger UDP discovery
        m_hub_found = false;
        m_hub_ip_configured = false;
        m_discovery_retries = 0;
        console.println("[tcp] Hub offline, starting UDP discovery...");
    }
}
```

### 7.4 Fallback

- Se UDP discovery falhar após 20 tentativas → usar IP fixo (HUB_IP_DEFAULT)
- Se HTTP falhar → retry com backoff

## 8. Arquivos de Implementação

| Arquivo | Descrição |
|---------|-----------|
| `hub/src/tcp_radio_handler.h` | Declaração da classe `TcpRadioHandler` |
| `hub/src/tcp_radio_handler.cpp` | Implementação: UDP server, HTTP endpoints, queue |
| `nodes/tcp/src/main.cpp` | Node TCP: UDP discovery, WiFi, registro, envio |
| `nodes/tcp/include/config.h` | Configurações: HUB_IP_DEFAULT, intervalos |
| `nodes/tcp/include/pages.h` | Dashboard HTML (PROGMEM) |
| `nodes/tcp/platformio.ini` | Build flags, lib_deps |
| `shared/src/tcp_protocol.h` | Structs UDP (GW_DISCOVER, GW_ANNOUNCE) |

## 9. Regras de Implementação

- [ ] `TCP_ENABLED` guarda todo código TCP no hub (compilação condicional)
- [ ] Loop non-blocking (sem `delay()`, regra 15)
- [ ] WiFi não-bloqueante (regra 26)
- [ ] Dashboard padrão (sidebar 180px, stats-header, polling 3s, regra 28)
- [ ] API endpoints obrigatórios: `/api/state`, `/api/settings`, `/api/wifi`, `/api/ota`, `/api/restart` (regra 29)
- [ ] Console/telnet obrigatório via `common_console.h` (regra 30)
- [ ] EEPROM_SIZE = 512 (regra 31)
- [ ] Validar strings EEPROM (regra 20)
- [ ] `FW_VERSION` = tag atual (regra 13)
- [ ] `lib_extra_dirs` apontando para `../../shared` (regra 14)
- [ ] UDP discovery reutiliza porta 5000 do bridge
- [ ] Fallback IP fixo se UDP discovery falhar
