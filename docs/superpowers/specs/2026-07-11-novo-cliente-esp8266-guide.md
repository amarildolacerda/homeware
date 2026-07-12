# Guia: Criar Novo Cliente ESP8266

Referência consolidada para criação de novos clientes ESP8266 no projeto Homeware. Aplica-se a AI e desenvolvedores humanos.

## 1. Checklist de Decisão (antes de começar)

- [ ] **Tipo de dispositivo**: sensor (só leitura), atuador (relé/LED), ou misto?
- [ ] **Repeater**: o cliente deve funcionar como extensor ESP-NOW bidirecional? Se sim, seguir regras do `clients/esp8266_repeater`.
- [ ] **Alexa/Espalexa**: necessário controle por voz? (ver `esp8266_lampada`)
- [ ] **Timer/Agendamento**: necessário programação de horários? (ver `esp8266_onoff`)
- [ ] **Sensor type**: definir enum em `espnow_protocol.h` — usar valor ainda não existente
- [ ] **Payload**: definir struct `payload_<nome>_t` com dados do sensor
- [ ] **GPIO pins**: mapear pinos físicos (A0, D1-D8) para funções
- [ ] **Nome do cliente**: padrão `esp8266_<nome>` (ex: `esp8266_chuva`, `esp8266_dht_gas`)
- [ ] **Dashboard**: definir hero stat principal (badge grande) + 1 stat no header

## 2. Estrutura do Projeto

```
clients/esp8266_<nome>/
├── include/
│   ├── config.h              # FW_VERSION, pinos, intervals, EEPROM
│   ├── espnow_protocol.h     # Subset do protocolo (copiar do gateway)
│   ├── pages.h               # Dashboard HTML + /docs (PROGMEM)
│   └── console.h             # Classe ConsoleOutput
├── src/
│   ├── main.cpp              # Setup/loop, ESP-NOW, web server, sensor
│   └── console.cpp           # Serial + Telnet output
├── platformio.ini            # Duas envs: esp8266 (serial) + esp8266_ota
├── README.md                 # Hardware, wiring, API, OTA, shortcuts
├── SPEC.md                   # Especificação completa
├── build.sh                  # Build
├── flash.sh                  # Source + build + flash (porta padrão /dev/ttyUSB0)
├── flash.ps1                 # Flash serial/OTA Windows
├── monitor.sh                # Source + monitor (Ctrl+] para sair)
├── erase.ps1                 # Erase flash Windows
└── .gitignore
```

### Arquivos obrigatórios

| Arquivo | Obrigatório | Função |
|---------|:-----------:|--------|
| README.md | ✅ | Wiring, pinos, atalhos, OTA |
| SPEC.md | ✅ | Especificação de funcionamento |
| config.h | ✅ | Constantes e configurações |
| espnow_protocol.h | ✅ | Structs e enums do protocolo |
| pages.h | ✅ | Dashboard HTML + /docs |
| console.h/cpp | ✅ | Terminal serial + telnet |
| platformio.ini | ✅ | Build environments |
| main.cpp | ✅ | Lógica principal |

## 3. config.h

Todas as constantes ficam aqui. Exemplo template:

```cpp
#pragma once
#include <Arduino.h>

// Firmware - deve ser igual à tag git
#define FW_VERSION "v0.0.22"
#define DEVICE_NAME "Nome do Dispositivo"

// Intervalos (ms)
#define STATE_UPDATE_INTERVAL 5000      // Leitura + envio de dados
#define STATE_SEND_THRESHOLD 5           // Diferença mínima para enviar
#define STATE_FORCE_INTERVAL 300000      // Envio forçado (5 min)
#define HEARTBEAT_INTERVAL 60000         // Heartbeat a cada 60s

// Pinos - mapear conforme hardware
#define SENSOR_PIN A0                    // Pino do sensor principal
#define SENSOR_DIGITAL_PIN 16            // Pino digital (se aplicável)
#define LED_PIN 2                        // LED built-in (GPIO2, active LOW)
#define RELAY_PIN 12                     // Relé (se aplicável)

// LED blink intervals
#define LED_BLINK_WIFI_MS 200            // WiFi offline
#define LED_BLINK_GATEWAY_MS 2000        // Não pareado

// ESP-NOW
#define ESPNOW_CHANNEL 1                 // Canal fixo
#define ESPNOW_SEND_RETRIES 3            // Máximo de retries
#define ESPNOW_ACK_TIMEOUT_MS 200        // Timeout ACK
#define ESPNOW_PAIR_INTERVAL_MS 10000    // Intervalo de pareamento
#define ESPNOW_MAX_PAIR_ATTEMPTS 30      // Máximo tentativas

// WiFiManager
#define WIFI_CONFIG_PORTAL_SSID "ESP8266_<Nome>"
#define WIFI_CONFIG_PORTAL_PASS "password123"
```

### Regras config.h
- `FW_VERSION` = tag git (unificada entre gateway + bridge + clients)
- Não usar `delay()` — tudo via `millis()` timestamps
- EEPROM layout: endereço 0 = MAC gateway (magic 0xAA + 6 bytes), endereço 10 = device name (48 bytes)

## 4. espnow_protocol.h

Subset do protocolo do gateway. Copiar do `gateway/include/espnow_protocol.h` os tipos necessários.

### Structs obrigatórias

```cpp
// Header comum a todas as mensagens
typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t sensor_type;
    uint8_t battery_pct;
    int16_t rssi;
    uint8_t payload_len;
    uint8_t payload[ESPNOW_MAX_PAYLOAD - 18];
} espnow_header_t;

// Payload do sensor (definir conforme dispositivo)
typedef struct __attribute__((packed)) {
    uint8_t sensor_data_field;   // ← substituir pelos campos reais
    // ... mais campos
} payload_<nome>_t;

// ACK do gateway
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t status;      // PAIR_STATUS_OK | PAIR_STATUS_FULL | PAIR_STATUS_DENIED
    uint8_t assigned_slot;
} espnow_ack_t;

// Pedido de pareamento
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t sensor_type;
    uint8_t firmware_version[4];
    char device_name[16];
} espnow_pair_request_t;

// Resposta do gateway
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t gateway_mac[6];
    uint8_t status;
    uint16_t assigned_slot;
} espnow_pair_response_t;
```

### Msg types disponíveis

| Enum | Valor | Uso |
|------|-------|-----|
| `ESPNOW_MSG_SENSOR_DATA` | 0x01 | Dados do sensor |
| `ESPNOW_MSG_PAIR_REQUEST` | 0x02 | Pedido de pareamento |
| `ESPNOW_MSG_PAIR_RESPONSE` | 0x03 | Resposta do gateway |
| `ESPNOW_MSG_ACK` | 0x04 | Confirmação |
| `ESPNOW_MSG_HEARTBEAT` | 0x05 | Heartbeat |
| `ESPNOW_MSG_OTA_TRIGGER` | 0x06 | Trigger OTA |
| `ESPNOW_MSG_COMMAND` | 0x07 | Comando do gateway |
| `ESPNOW_MSG_TIME_SYNC` | 0x08 | Sincronização de tempo |
| `ESPNOW_MSG_GW_ANNOUNCE` | 0x09 | Gateway announce |
| `ESPNOW_MSG_GW_DISCOVER` | 0x0A | Descoberta gateway |
| `ESPNOW_MSG_REPEATER_STATUS` | 0x0B | Status do repeater |

### Sensor types disponíveis

| Enum | Valor | Cliente |
|------|-------|---------|
| `SENSOR_TYPE_TEMP_HUM` | 1 | (reservado) |
| `SENSOR_TYPE_CONTACT` | 2 | (reservado) |
| `SENSOR_TYPE_MOTION` | 3 | (reservado) |
| `SENSOR_TYPE_GAS` | 4 | (reservado) |
| `SENSOR_TYPE_RAIN` | 5 | esp8266_chuva |
| `SENSOR_TYPE_TANK` | 6 | (reservado) |
| `SENSOR_TYPE_DHT_GAS` | 7 | esp8266_dht_gas |
| `SENSOR_TYPE_ONOFF` | 8 | esp8266_onoff |
| `SENSOR_TYPE_LIGHT` | 9 | esp8266_lampada |
| `SENSOR_TYPE_REPEATER` | 10 | esp8266_repeater |

> **Importante**: adicionar o novo `SENSOR_TYPE_<NOME>` no `espnow_protocol.h` do **gateway** também.

### Fluxo de Registro

```
Client                          Gateway
  │                                │
  │──── PAIR_REQUEST ─────────────>│  (broadcast, a cada 10s)
  │<─── PAIR_RESPONSE ────────────│  (contém assigned_slot + gateway_mac)
  │                                │
  │  [salva gateway MAC em EEPROM] │
  │                                │
  │──── SENSOR_DATA ──────────────>│  (periódico, a cada STATE_UPDATE_INTERVAL)
  │<─── ACK ──────────────────────│  (confirma recebimento)
  │                                │
  │──── HEARTBEAT ────────────────>│  (a cada 60s)
  │<─── ACK ──────────────────────│
```

- Se ACK = `PAIR_STATUS_DENIED`: marca como não pareado, reinicia pareamento
- Retry de registro no `loop()`, não só no `setup()`
- Broadcast MAC: `FF:FF:FF:FF:FF:FF`

## 5. Dashboard HTML (pages.h)

### Layout padrão

```
┌─────────────┬──────────────────────────────────┐
│ Sidebar     │ Stats Header: [RX] [TX] [Mem] [X]│
│ (180px)     ├──────────────────────────────────┤
│             │                                  │
│ 🏠 Home     │         HERO (badge grande)      │
│ 📋 Props    │                                  │
│ ⚙ Config   │     [Seção detalhes collapsível] │
│             │                                  │
│ v0.0.22     ├──────────────────────────────────┤
└─────────────┤ Footer: [dot gateway] [hora] [up]│
              └──────────────────────────────────┘
```

### CSS Variables (obrigatórias)

```css
:root {
  --bg: #f4f4f4;
  --surface: #fff;
  --surface-2: #f9fafb;
  --text: #1f2937;
  --muted: #6b7280;
  --muted-subtle: #9ca3af;
  --primary: #5e6ad2;
  --primary-strong: #828fff;
  --primary-focus: #eef0ff;
  --border: #e5e7eb;
  --success: #16a34a;
  --danger: #dc2626;
  --warn: #f59e0b;
  --info: #2563eb;
  --sidebar-w: 180px;
}
```

### Responsividade

- **Desktop**: sidebar 180px, main com `margin-left`
- **Mobile** (≤600px): sidebar 48px, só ícones, sem texto
- Usar `@media(max-width:600px)` para ajustes

### Seções do Dashboard

1. **Sidebar**: nome do dispositivo, ID, navegação (Home, Propriedades, Configurações), versão
2. **Stats Header**: RX, TX, Memória livre, + 1 stat específico do sensor
3. **Hero**: display grande do valor principal (badge colorido por estado)
4. **Detalhes (collapsible)**: Gateway, RSSI, IP, Uptime, Bateria, Versão
5. **Configurações**: campo device_name + botão salvar
6. **Footer**: dot verde/vermelho (gateway status), horário (JS), uptime

### /docs endpoint

Obrigatório — listar endpoints disponíveis:

```
GET /              → Dashboard
GET /docs          → Esta documentação
GET /api/state     → JSON estado completo
GET /api/settings  → Configurações atuais
POST /api/settings → Alterar configurações
POST /api/ota      → Upload firmware
```

### Polling pattern

```javascript
setInterval(() => {
  fetch('/api/state')
    .then(r => r.json())
    .then(data => {
      // atualizar DOM
    });
}, 3000); // 3 segundos
```

### Regras de memória

- Dashboard via `send_P()` com `FPSTR()` para strings PROGMEM
- Se dashboard > 8KB: usar chunks via `WiFiClient` com `yield()` entre chunks
- Nunca alocar String grande na heap (usar `const char[]` PROGMEM)

## 6. Console (console.h / console.cpp)

### Módulo compartilhado

Todos os clientes usam o mesmo padrão de console:

```cpp
// console.h
class ConsoleOutput {
public:
  void begin();
  void loop();
  void printf(const char* fmt, ...);
  bool telnet_available();
  char telnet_read();
};
```

### Atalhos de teclado obrigatórios

| Tecla | Ação |
|-------|------|
| `l` | Leitura forçada + envio ao gateway |
| `s` | Status do dispositivo |
| `p` | Resetar par e tentar parear |
| `u` | Info OTA |
| `h` / `?` | Help |
| `r` | Restart |

- Atalhos extras conforme necessidade do sensor/atuador
- Atalhos de teclado no terminal (conforme AGENTS.md regra 4)

## 7. Documentação

### README.md (obrigatório)

```markdown
# ESP8266 <Nome> - <Descrição>

## Hardware
| Componente | GPIO | Notas |
|------------|------|-------|
| Sensor X   | A0   | ... |
| LED        | D4/GPIO2 | Active LOW |

## Wiring
[Diagrama ou tabela de conexões]

## Compilação
[PlatformIO, build.sh, flash.sh]

## OTA
[ Como atualizar via rede ]

## Terminal / Atalhos
[ Tabela de comandos ]

## API
[ Endpoints disponíveis ]
```

### SPEC.md (obrigatório)

Seguir estrutura do `esp8266_chuva/SPEC.md`:

1. **Visão Geral** — descrição do dispositivo
2. **Fluxo de Inicialização** — ordem de setup (HW → WiFi → ESP-NOW → Gateway → Web → OTA)
3. **Componentes Funcionais** — ESP-NOW, Sensor, Dashboard, Terminal
4. **Loop (Non-blocking)** — máquina de estados com timestamps
5. **Regras Importantes** — regras específicas do cliente
6. **Hardware** — tabela de GPIOs
7. **LED Status** — comportamento do LED
8. **Dependências** — libs PlatformIO

## 8. Regras e Boas Práticas

### Non-blocking loop

- **NENHUM** `delay()` no `loop()` (exceto WiFi reconnect)
- Usar `millis()` + timestamps para: envio, ACK wait, retries, pareamento, heartbeat, LED
- Padrão de máquina de estados:
- `setup` nao bloqueante, `loop` não bloqueante
-  **Páginas web PROGMEM**: páginas HTML grandes (>10KB) via `FPSTR` + `send()` estouram heap no ESP8266 porque alocam String RAM. `send_P()` e `sendContent_P()` também falham se o buffer TCP encher (`write()` retorna 0). Para páginas grandes, escrever response manualmente via `WiFiClient` em chunks pequenos (256 bytes) com `yield()` entre chunks. Alternativa: manter páginas enxutas (<8KB) para usar `send_P()` sem risco.

```cpp
static unsigned long lastSend = 0;
void loop() {
  unsigned long now = millis();

  if (now - lastSend >= STATE_UPDATE_INTERVAL) {
    lastSend = now;
    readSensor();
    sendToGateway();
  }

  checkAck();      // não bloqueante
  checkPairing();  // não bloqueante
  checkHeartbeat();
  updateLed();
}
```

### Memória / PROGMEM

- Páginas HTML grandes: usar `send_P()` ou chunks via `WiFiClient`
- Strings constantes: `const char x[] PROGMEM`
- Nunca usar `String` para payloads grandes

### cJSON / JSON

- **SEMPRE** copiar `valuestring` para buffer local com `strncpy` antes de `cJSON_Delete`
- Nunca usar pointer direto de cJSON após delete

### EEPROM

- Layout: endereço 0 = MAC gateway (magic 0xAA + 6 bytes), endereço 10+ = device name (48 bytes)
- Validar device name: > 32, < 127 (ASCII)
- Salvar estado de relé/atuador na EEPROM para persistir entre reboots

### Versão unificada

- `FW_VERSION` em `config.h` = tag git
- Todos os dispositivos (gateway + bridge + clients) devem ter a mesma versão

### Feedback imediato

- Qualquer mudança de estado no client deve disparar envio ao gateway imediatamente
- Padrão: setar `s_last_espnow_send = 0` para forçar envio no próximo ciclo

### Canal ESP-NOW

- Canal fixo em **1** — nunca alterar

### Device ID

- Dinâmico: `esp8266_<chip_id>` — não configurável
- Device name: configurável via WiFiManager, salvo em EEPROM

### Persistência em NVS

- Devices bridgeados devem ser persistidos em NVS para restaurar no boot

### Bridge_connected

- Clients enviam `bridge_connected` no `/api/state`

## 9. Dependências PlatformIO

### platformio.ini template

```ini
[env:esp8266]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson@^7.3.1
    tzapu/WiFiManager@^2.0.16
    ; libs extras conforme necessário (DHT, Espalexa, etc)

[env:esp8266_ota]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
upload_protocol = espota
upload_port = 192.168.x.x  ; IP do dispositivo
lib_deps =
    bblanchon/ArduinoJson@^7.3.1
    tzapu/WiFiManager@^2.0.16
```

### Libs comuns

| Lib | Uso |
|-----|-----|
| `ArduinoJson` ^7.3.1 | JSON parsing/serialization |
| `WiFiManager` ^2.0.16 | Configuração WiFi via portal |
| `DHT sensor library` | Sensores DHT (se aplicável) |
| `Espalexa` | Controle Alexa (se aplicável) |

## 10. Passo a Passo Resumido

1. **Decidir**: tipo, repeater, Alexa, timer, sensor type, payload
2. **Criar pasta** `clients/esp8266_<nome>/` com estrutura de pastas
3. **Criar `config.h`**: FW_VERSION, pinos, intervals
4. **Criar `espnow_protocol.h`**: subset do gateway + novo payload struct + sensor type
5. **Criar `console.h/cpp`**: copiar de cliente existente
6. **Criar `pages.h`**: dashboard com layout padrão (sidebar, hero, collapsible, footer)
7. **Criar `main.cpp`**: setup (WiFi → ESP-NOW → Web → OTA) + loop (non-blocking)
8. **Criar `platformio.ini`**: env serial + env OTA
9. **Criar `README.md`**: hardware, wiring, API, OTA, shortcuts
10. **Criar `SPEC.md`**: seguindo estrutura padrão
11. **Criar scripts** bash/ps1: build, flash, monitor, erase
12. **Atualizar gateway**: adicionar `SENSOR_TYPE_<NOME>` no `espnow_protocol.h`
13. **Testar**: build, flash, monitor, verificar registro no gateway
14. **Criar branch**, commit, push

## Referências

- `clients/esp8266_chuva/` — cliente sensor estável (referência principal)
- `clients/esp8266_dht_gas/` — sensor DHT22 + MQ-2 (desenvolvimento)
- `clients/esp8266_lampada/` — relé + Alexa + repeater
- `clients/esp8266_onoff/` — relé + timer + repeater
- `clients/esp8266_repeater/` — extensor de alcance
- `gateway/include/espnow_protocol.h` — protocolo canônico/estendido
- `docs/superpowers/specs/` — specs de features existentes
