# SPEC: Integração Telegram Bot — Hub AgriSense

## 1. Visão Geral

### 1.1 Objetivo
Implementar um bot interativo no Telegram que permite ao usuário:
- Receber notificações push (alarmes, device offline, bateria baixa)
- Controlar dispositivos remotamente (ligar/desligar relés)
- Consultar status de todos os nodes sensores
- Executar comandos úteis (restart, bateria)

### 1.2 Escopo
- **Incluso:** Bot Telegram no hub ESP32, comandos interativos, alertas automáticos
- **Excluso:** Integração com Google Home/Apple HomeKit (usar HA para não sobrecarregar memória)
- **Excluso:** OTA via Telegram (manter apenas no dashboard por segurança)

### 1.3 Pré-requisitos
- Hub ESP32 com WiFi conectado à internet
- Conta Telegram (gratuita)
- Bot criado via @BotFather (token + username)

---

## 2. Arquitetura

```
┌─────────────────────────────────────────────────────────────────┐
│                        HUB ESP32                                │
│                                                                 │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │ ESP-NOW      │    │ LoRa         │    │ TCP          │      │
│  │ Handler      │    │ Handler      │    │ Handler      │      │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘      │
│         │                   │                   │               │
│         └───────────────────┼───────────────────┘               │
│                             ▼                                   │
│                    ┌────────────────┐                           │
│                    │ Sensor Registry│                           │
│                    └────────┬───────┘                           │
│                             │                                   │
│         ┌───────────────────┼───────────────────┐               │
│         ▼                   ▼                   ▼               │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │ MQTT Client  │    │ Web Server   │    │ Telegram Bot │◄─NOVO│
│  │ (HA)         │    │ (Dashboard)  │    │ (Notifications│      │
│  └──────────────┘    └──────────────┘    │  + Control)  │      │
│                                          └──────┬───────┘      │
│                                                 │               │
└─────────────────────────────────────────────────┼───────────────┘
                                                  │
                                                  ▼ HTTPS
                                        ┌──────────────────┐
                                        │ Telegram Servers │
                                        │ api.telegram.org │
                                        └────────┬─────────┘
                                                 │
                                                 ▼
                                        ┌──────────────────┐
                                        │   Usuário        │
                                        │   (Celular)      │
                                        └──────────────────┘
```

---

## 3. Configuração

### 3.1 Variáveis de Build (`platformio.ini`)

```ini
[env:hub_32_lora_heltec]
build_flags =
    ${env:hub_lora.build_flags}
    -D TELEGRAM_ENABLED                    ; Ativa módulo Telegram
    -D BOT_TOKEN=\"1234567890:ABCdefGHI...\"  ; Token do BotFather
    -D CHAT_ID=\"987654321\"                ; Chat ID do usuário autorizado
    -D TELEGRAM_POLL_INTERVAL_MS=2000      ; Intervalo de polling (2s)
    -D TELEGRAM_LONG_POLL_SECONDS=60       ; Long poll (60s)
    -D TELEGRAM_MAX_MESSAGE_LEN=4096       ; Tamanho máximo de mensagem

lib_deps =
    ${env:hub_lora.lib_deps}
    witnessmenow/UniversalTelegramBot@^1.3.0
```

### 3.2 Configuração Múltiplos Usuários (Futuro)

```cpp
// Em config.h ou via EEPROM
struct telegram_config_t {
    bool enabled;
    char bot_token[64];
    char chat_ids[4][20];       // Até 4 usuários autorizados
    uint8_t num_authorized;
    uint32_t poll_interval_ms;
    uint32_t long_poll_seconds;
};
```

---

## 4. Comandos do Bot

### 4.1 Comandos Implementados

| Comando | Descrição | Exemplo Resposta |
|---------|-----------|------------------|
| `/start` | Mensagem de boas-vindas + lista de comandos | "Bem-vindo ao AgriSense! Comandos disponíveis..." |
| `/help` | Lista detalhada de comandos | Lista formatada com descrições |
| `/status` | Status geral do hub (inclui uptime, IP, memória) | "📊 Status do Hub\nDevice: ...\nIP: ...\nUptime: 3d 12h 45min" |
| `/list` | Lista todos os nodes pareados | "📋 Nodes Pareados\n🟢 1. bomba_1 (TCP) - on/off\n🔴 2. sensor_temp - offline" |
| `/on <node>` | Liga relé do node | "✅ bomba_1 ligado!" |
| `/off <node>` | Desliga relé do node | "❌ bomba_1 desligado!" |
| `/battery` | Níveis de bateria de todos os nodes | "🔋 Níveis de Bateria\n✅ bomba_1: 85%\n🟡 sensor_temp: 42%" |

### 4.2 Comandos Futuros (Fase 2)

| Comando | Descrição | Status |
|---------|-----------|--------|
| `/restart <node>` | Reinicia um node remotamente | ⏳ Pendente |
| `/heap` | Memória livre do hub | ⏳ Pendente |
| `/debug` | Informações de debug | ⏳ Pendente |
| `/logs` | Últimos 5 eventos do log | ⏳ Pendente |
| `/alerts` | Mostra configuração de alertas | ⏳ Pendente |
| `/alerts on <tipo>` | Ativa alerta do tipo | ⏳ Pendente |
| `/alerts off <tipo>` | Desativa alerta do tipo | ⏳ Pendente |

---

## 5. Notificações Automáticas

### 5.1 Classificação de Mensagens

| Nível | Emoji | Prioridade | Ação | Throttle |
|-------|-------|------------|------|----------|
| **CRITICAL** | 🔴 | Máxima | Ação imediata necessária | Sem throttle (sempre enviar) |
| **ALERT** | ⚠️ | Alta | Ação em breve necessária | 5-60 min |
| **WARNING** | 🟡 | Média | Atenção necessária | 1-6 horas |
| **INFO** | 🟢 | Baixa | Apenas informativo | 5-60 min |

### 5.2 Mensagens por Nível

#### 🔴 CRITICAL (Crítico)
- Gás detectado acima do limite
- Fumaça detectada
- Device offline há mais de 30 min
- Bateria crítica (< 10%)
- Erro de sistema grave

#### ⚠️ ALERT (Alerta)
- Gás elevado (próximo do limite)
- Device offline (primeiros 5 min)
- Temperatura muito alta/baixa
- Umidade fora do normal
- Conexão MQTT perdida

#### 🟡 WARNING (Aviso)
- Bateria baixa (< 20%)
- RSSI fraco (< -80 dBm)
- Device com heap baixo
- OTA disponível
- Configuração alterada

#### 🟢 INFO (Informativo)
- Device reconectou
- Resumo diário de sensores
- Status do hub
- Comando executado com sucesso
- Versão de firmware atualizada

### 5.3 Exemplos de Mensagens

| Tipo | Mensagem | Nível |
|------|----------|-------|
| Gás alto | "🔴 GÁS CRÍTICO: 500ppm no galpão_2!" | CRITICAL |
| Fumaça | "🔴 FOGO DETECTADO em sensor_fumaça!" | CRITICAL |
| Offline longo | "🔴 Node 'bomba_1' offline há 35min" | CRITICAL |
| Bateria crítica | "🔴 Bateria CRÍTICA: 8% em bomba_1" | CRITICAL |
| Gás elevado | "⚠️ Gás elevado: 380ppm no galpão_2" | ALERT |
| Offline | "⚠️ Node 'sensor_temp' ficou offline" | ALERT |
| Temp alta | "⚠️ Temperatura alta: 42°C em dht_1" | ALERT |
| MQTT off | "⚠️ Conexão MQTT perdida" | ALERT |
| Bateria baixa | "🟡 Bateria baixa: 18% em presenca_1" | WARNING |
| RSSI fraco | "🟡 Sinal fraco: -85dBm em sensor_chuva" | WARNING |
| Heap baixo | "🟡 Memória baixa: 45KB livre" | WARNING |
| Device online | "🟢 Node 'bomba_1' reconectou!" | INFO |
| Resumo diário | "🟢 Resumo: Temp: 28°C | Umid: 65% | Chuva: 0mm" | INFO |
| Comando ok | "🟢 Bomba_1 ligada com sucesso" | INFO |
| Status | "🟢 Hub online | 5 nodes | Uptime: 3d 12h" | INFO |

### 5.4 Throttling (Anti-Spam)

```cpp
// Limites de envio por nível de prioridade
#define TELEGRAM_THROTTLE_CRITICAL_MS   0       // Sem throttle (sempre enviar)
#define TELEGRAM_THROTTLE_ALERT_MS      300000  // 5 min entre alertas
#define TELEGRAM_THROTTLE_WARNING_MS     3600000 // 1 hora entre avisos
#define TELEGRAM_THROTTLE_INFO_MS       300000  // 5 min entre informativos

// Limites por tipo específico (override do nível)
#define TELEGRAM_THROTTLE_GAS_MS        60000   // 1 min entre alertas de gás
#define TELEGRAM_THROTTLE_OFFLINE_MS    300000  // 5 min entre alertas de offline
#define TELEGRAM_THROTTLE_BATTERY_MS    3600000 // 1 hora entre alertas de bateria
```

### 5.5 Estados de Alerta

```cpp
enum alert_state_t {
    ALERT_IDLE,         // Aguardando condição
    ALERT_TRIGGERED,    // Condição atingida, alerta enviado
    ALERT_COOLDOWN,     // Em cooldown, não enviar novamente
    ALERT_CLEARED       // Condição normalizada, alerta de "tudo OK"
};
```

---

## 6. Segurança

### 6.1 Autenticação

```cpp
// Whitelist de chat IDs autorizados
const long AUTHORIZED_USERS[] = {987654321, 123456789};
const int NUM_AUTHORIZED_USERS = 2;

bool isAuthorized(long chat_id) {
    for (int i = 0; i < NUM_AUTHORIZED_USERS; i++) {
        if (AUTHORIZED_USERS[i] == chat_id) return true;
    }
    return false;
}
```

### 6.2 Resposta para Não Autorizados

```cpp
if (!isAuthorized(chat_id)) {
    bot.sendMessage(chat_id, 
        "❌ Acesso não autorizado.\n"
        "Seu Chat ID: " + String(chat_id) + "\n"
        "Adicione este ID na whitelist do hub.", "");
    return;
}
```

### 6.3 Comandos Restritos

| Comando | Permissão |
|---------|-----------|
| `/on`, `/off`, `/restart` | Apenas usuários autorizados |
| `/status`, `/list`, `/battery` | Apenas usuários autorizados |
| `/debug`, `/heap`, `/ip` | Apenas usuários autorizados |
| `/alerts` (configuração) | Apenas administrador (primeiro da whitelist) |

### 6.4 Token Security

- Token armazenado em `build_flags` (não em código fonte)
- Não logar token em mensagens de debug
- Não enviar token em mensagens Telegram
- Token pode ser revogado via @BotFather

---

## 7. Arquivos

### 7.1 Novos Arquivos

| Arquivo | Descrição | Tamanho Estimado |
|---------|-----------|------------------|
| `hub/include/telegram_bot.h` | Declarações, structs, enums | ~2KB |
| `hub/src/telegram_bot.cpp` | Implementação completa | ~15-20KB |

### 7.2 Arquivos Modificados

| Arquivo | Mudança |
|---------|---------|
| `hub/platformio.ini` | Adicionar lib_deps e build_flags |
| `hub/src/main.cpp` | Chamar `telegram_bot_init()` e `telegram_bot_loop()` |
| `hub/src/espnow_handler.cpp` | Chamar `telegram_alert_offline()` / `telegram_alert_online()` |
| `hub/src/tcp_radio_handler.cpp` | Chamar `telegram_alert_offline()` / `telegram_alert_online()` |
| `hub/src/mqtt_client.cpp` | Opcional: callbacks para alertas MQTT |

---

## 8. Estrutura de Dados

### 8.1 Estado do Bot

```cpp
struct telegram_bot_state_t {
    bool enabled;
    bool initialized;
    unsigned long last_poll_ms;
    unsigned long last_status_ms;
    unsigned long last_daily_report_ms;
    int messages_processed;
    int alerts_sent;
    int errors_count;
    unsigned long last_error_ms;
};
```

### 8.2 Configuração de Alertas

```cpp
struct telegram_alert_config_t {
    bool gas_enabled;
    bool offline_enabled;
    bool battery_enabled;
    bool temperature_enabled;
    bool daily_report_enabled;
    uint8_t daily_report_hour;     // 0-23
    uint8_t battery_threshold;     // % (ex: 20)
    float temp_high_threshold;     // °C (ex: 35.0)
    float temp_low_threshold;      // °C (ex: 10.0)
};
```

### 8.3 Throttle por Sensor

```cpp
struct telegram_throttle_t {
    unsigned long last_gas_alert_ms;
    unsigned long last_offline_alert_ms;
    unsigned long last_battery_alert_ms;
    unsigned long last_online_alert_ms;
};
```

---

## 9. Loop Principal

### 9.1 Fluxo de Execução

```
telegram_bot_loop() {
    1. Verificar se está habilitado (TELEGRAM_ENABLED)
    2. Verificar intervalo de polling (TELEGRAM_POLL_INTERVAL_MS)
    3. Chamar bot.getUpdates() (long poll ou short poll)
    4. Processar mensagens recebidas:
       a. Validar chat_id (isAuthorized)
       b. Parsear comando (/start, /on, /off, etc.)
       c. Executar ação (ligar relé, consultar status, etc.)
       d. Enviar resposta
    5. Verificar alertas pendentes:
       a. Verificar timeouts de sensores (offline)
       b. Verificar thresholds (gás, temperatura, bateria)
       c. Verificar throttle (anti-spam)
       d. Enviar alertas se necessário
    6. Verificar relatório diário (horário configurado)
    7. Atualizar estatísticas (messages_processed, alerts_sent)
}
```

### 9.2 Integração com Loop do Hub

```cpp
// main.cpp
void loop() {
    // ... código existente ...
    
    #if defined(TELEGRAM_ENABLED)
        telegram_bot_loop();
    #endif
    
    // ... código existente ...
}
```

### 9.3 Non-Blocking

- Polling com intervalo configurável (2s padrão)
- Long poll (60s) reduz requests mas bloqueia ~60s (usar com cuidado)
- Alternativa: short poll (2s) + intervalo maior entre polls
- Não usar `delay()` — usar `millis()` para timing

---

## 10. Integração com Sensores

### 10.1 Callbacks de Alerta

```cpp
// Chamado quando sensor fica offline (espnow_handler.cpp, tcp_radio_handler.cpp)
void telegram_alert_offline(virtual_sensor_t* sensor) {
    if (!telegram_config.offline_enabled) return;
    if (throttle_check(&throttle.last_offline_alert_ms, TELEGRAM_THROTTLE_OFFLINE_MS)) return;
    
    String msg = "❌ Node '" + String(sensor->name) + "' ficou offline!";
    bot.sendMessage(CHAT_ID, msg, "");
    throttle.last_offline_alert_ms = millis();
}

// Chamado quando sensor reconecta
void telegram_alert_online(virtual_sensor_t* sensor) {
    if (!telegram_config.offline_enabled) return;
    if (throttle_check(&throttle.last_online_alert_ms, 5000)) return; // 5s cooldown
    
    String msg = "✅ Node '" + String(sensor->name) + "' reconectou!";
    bot.sendMessage(CHAT_ID, msg, "");
    throttle.last_online_alert_ms = millis();
}
```

### 10.2 Verificação de Thresholds

```cpp
void telegram_check_thresholds() {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t* s = sensor_registry_get(i);
        if (!s || !s->paired || !s->online) continue;
        
        // Gás
        if (telegram_config.gas_enabled && s->state.dht_gas.gas_ppm > THRESHOLD_GAS) {
            if (throttle_check(&throttle.last_gas_alert_ms, TELEGRAM_THROTTLE_GAS_MS)) {
                String msg = "⚠️ GÁS ALTO: " + String(s->state.dht_gas.gas_ppm) + "ppm no " + String(s->name) + "!";
                bot.sendMessage(CHAT_ID, msg, "");
                throttle.last_gas_alert_ms = millis();
            }
        }
        
        // Bateria
        if (telegram_config.battery_enabled && s->battery_pct < telegram_config.battery_threshold) {
            if (throttle_check(&throttle.last_battery_alert_ms, TELEGRAM_THROTTLE_BATTERY_MS)) {
                String msg = "🔋 Bateria baixa: " + String(s->name) + " com " + String(s->battery_pct) + "%";
                bot.sendMessage(CHAT_ID, msg, "");
                throttle.last_battery_alert_ms = millis();
            }
        }
        
        // Temperatura (se DHT)
        if (telegram_config.temperature_enabled && s->type == SENSOR_TYPE_DHT_GAS) {
            float temp = s->state.dht_gas.temperature;
            if (temp > telegram_config.temp_high_threshold) {
                if (throttle_check(&throttle.last_gas_alert_ms, TELEGRAM_THROTTLE_GAS_MS)) {
                    String msg = "🌡️ Temperatura alta: " + String(s->name) + " com " + String(temp, 1) + "°C";
                    bot.sendMessage(CHAT_ID, msg, "");
                }
            }
        }
    }
}
```

---

## 11. Tratamento de Erros

### 11.1 Erros de Conexão

```cpp
void telegram_handle_error(const char* error) {
    console.printf("[Telegram] Erro: %s\n", error);
    state.errors_count++;
    state.last_error_ms = millis();
    
    // Se muitos erros consecutivos, desabilitar temporariamente
    if (state.errors_count > 10) {
        console.println("[Telegram] Muitos erros — desabilitando por 5 minutos");
        state.enabled = false;
        state.last_error_ms = millis(); // Para reabilitar depois
    }
}
```

### 11.2 Fallback

- Se Telegram falhar, notificações continuam via MQTT (Home Assistant)
- Dashboard local continua funcionando
- Console/telnet continua disponível
- Erros são logados mas não bloqueiam o hub

### 11.3 Recovery

```cpp
// Reabilitar após 5 minutos de silêncio
if (!state.enabled && (millis() - state.last_error_ms > 300000)) {
    console.println("[Telegram] Reabilitando após cooldown");
    state.enabled = true;
    state.errors_count = 0;
}
```

---

## 12. Memória

### 12.1 Consumo Estimado

| Componente | RAM Estática | RAM Dinâmica |
|------------|--------------|--------------|
| UniversalTelegramBot | ~2KB | ~3-5KB |
| WiFiClientSecure (TLS) | ~4KB | ~20-30KB |
| Código do bot | ~1KB | ~2-3KB |
| **Total** | **~7KB** | **~25-38KB** |

### 12.2 Otimizações

1. **Long Polling** — reduz requests, economiza CPU
2. **TLS Session Reuse** — reutiliza sessão SSL
3. **PROGMEM** — textos em flash
4. **Desabilitação dinâmica** — pausa se `free_heap < 50KB`

### 12.3 Monitoramento

```cpp
// No comando /heap
void handleHeap(long chat_id) {
    String msg = "📊 Memória:\n";
    msg += "Free heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    msg += "Heap total: " + String(ESP.getHeapSize()) + " bytes\n";
    msg += "Uso: " + String(100 - (ESP.getFreeHeap() * 100 / ESP.getHeapSize())) + "%";
    bot.sendMessage(chat_id, msg, "");
}
```

---

## 13. Testes

### 13.1 Testes Manuais

| Cenário | Ação Esperada | Resultado |
|---------|---------------|-----------|
| Enviar `/start` | Receber lista de comandos | ✅ |
| Enviar `/status` | Receber status do hub | ✅ |
| Enviar `/on bomba_1` | Relé ligar + confirmação | ✅ |
| Enviar `/off bomba_1` | Relé desligar + confirmação | ✅ |
| Desligar node | Receber alerta de offline | ✅ |
| Reconectar node | Receber alerta de online | ✅ |
| Simular gás alto | Receber alerta de gás | ✅ |
| Enviar comando não autorizado | Receber erro de acesso | ✅ |
| Enviar `/battery` | Receber níveis de bateria | ✅ |
| Enviar `/heap` | Receber memória livre | ✅ |

### 13.2 Testes Automatizados (Futuro)

- Mock do Telegram Bot API
- Testes de timeout e throttle
- Testes de segurança (whitelist)

---

## 14. Exemplo de Interação

```
Usuário: /start
Bot: 🌱 Bem-vindo ao AgriSense!
     Comandos disponíveis:
     /status - Status geral
     /list - Listar nodes
     /on <node> - Lig relé
     /off <node> - Desligar relé
     /battery - Níveis de bateria
     /help - Ajuda completa

Usuário: /status
Bot: 📊 Hub AgriSense
     Status: online
     Nodes: 5 (3 online, 2 offline)
     LoRa: 2 nodes
     TCP: 3 nodes
     Uptime: 3d 12h 45min
     Free heap: 185KB

Usuário: /list
Bot: 📋 Nodes Pareados:
     1. bomba_1 (ON) - TCP
     2. sensor_temp (ON) - LoRa
     3. presenca_1 (OFF) - ESP-NOW
     4. sensor_chuva (ON) - LoRa
     5. extender_1 (ON) - TCP

Usuário: /on bomba_1
Bot: ✅ bomba_1 ligada!
     Estado: ON | GPIO: 4

Usuário: /battery
Bot: 🔋 Níveis de Bateria:
     bomba_1: 85% ✅
     sensor_temp: 42% ⚠️
     presenca_1: 15% 🔴
     sensor_chuva: 92% ✅

[Alerta automático - node offline]
Bot: ❌ Node 'presenca_1' ficou offline!
     Último visto: há 5 minutos

[Alerta automático - node reconectou]
Bot: ✅ Node 'presenca_1' reconectou!

Usuário: /heap
Bot: 📊 Memória:
     Free heap: 185.232 bytes
     Heap total: 327.680 bytes
     Uso: 43%
```

---

## 15. Limitações Conhecidas

1. **Latência** — Telegram polling tem latência de 1-2s (não é instantâneo)
2. **TLS** — Handshake SSL consome ~20-30KB de RAM
3. **Rate Limit** — Telegram limita a 30 mensagens/segundo (geralmente suficiente)
4. **Bots só respondem** — Bot não pode iniciar conversa (usuário deve enviar `/start` primeiro)
5. **Sem suporte a fotos** — ESP32 não tem câmera (futuro: ESP32-CAM)

---

## 16. Futuras Melhorias

1. **Inline Keyboards** — Botões interativos para ação rápida
2. **Callback Queries** — Respostas a botões pressionados
3. **Fotos** — Enviar fotos de ESP32-CAM (futuro)
4. **Grupos** — Enviar alertas para grupo compartilhado
5. **Comandos personalizados** — Configurar comandos via dashboard
6. **Webhook** — Usar webhook em vez de polling (requer servidor externo)
7. **Múltiplos bots** — Bot dedicado para cada tipo de alerta
8. **Integração com HA** — Receber comandos do HA via Telegram

---

## 17. Integração com Dashboard

### 17.1 Configuração via Dashboard

O usuário pode configurar a integração Telegram diretamente pelo dashboard do hub:

**Acessando:**
1. Abrir `http://<hub-ip>/settings`
2. Expandir a seção "Telegram"
3. Clicar em "Configurar Telegram"

### 17.2 Campos de Configuração

| Campo | Descrição | Obrigatório |
|-------|-----------|-------------|
| **Habilitado** | Liga/desliga a integração | Sim |
| **Bot Token** | Token obtido via @BotFather | Sim (se habilitado) |
| **Chat ID** | ID do chat/usuário no Telegram | Sim (se habilitado) |
| **Poll Interval** | Intervalo de polling em ms (1000-60000) | Não (padrão: 2000) |

#### Níveis de Alerta
| Campo | Descrição | Padrão |
|-------|-----------|--------|
| **Alert Critical** | Enviar mensagens CRITICAL (🔴) | true |
| **Alert Alert** | Enviar mensagens ALERT (⚠️) | true |
| **Alert Warning** | Enviar mensagens WARNING (🟡) | true |
| **Alert Info** | Enviar mensagens INFO (🟢) | true |

#### Tipos de Alerta
| Campo | Descrição | Padrão |
|-------|-----------|--------|
| **Alerta Gás** | Notificar quando gás detectado | true |
| **Alerta Fumaça** | Notificar quando fumaça detectada | true |
| **Alerta Offline** | Notificar quando node fica offline | true |
| **Alerta Reconexão** | Notificar quando node reconecta | true |
| **Alerta Bateria** | Notificar bateria baixa/crítica | true |
| **Alerta Temperatura** | Notificar temp alta/baixa | true |
| **Alerta Umidade** | Notificar umidade fora do normal | true |
| **Alerta RSSI** | Notificar sinal fraco | false |
| **Alerta Memória** | Notificar heap baixo | false |
| **Relatório Diário** | Enviar resumo às 08:00 | true |

### 17.3 Interface do Dashboard

```
┌─────────────────────────────────────────┐
│ Telegram                              ▼ │
├─────────────────────────────────────────┤
│ Status          │ Telegram ON      ✅   │
│ Chat ID         │ 987654321             │
│ Poll Interval   │ 2000ms                │
│                                         │
│ [Configurar Telegram]                   │
└─────────────────────────────────────────┘
```

### 17.4 Modal de Configuração

```
┌─────────────────────────────────────────┐
│ Configurar Telegram Bot                 │
├─────────────────────────────────────────┤
│                                         │
│ Habilitado: [Desligado] [Ligado ✓]     │
│                                         │
│ Bot Token:                              │
│ ┌─────────────────────────────────────┐ │
│ │ 1234567890:ABCdef...                │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ Chat ID:                                │
│ ┌─────────────────────────────────────┐ │
│ │ 987654321                           │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ Poll Interval (ms):                     │
│ ┌─────────────────────────────────────┐ │
│ │ 2000                                │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ Níveis de Alerta:                       │
│ ┌─────────────────────────────────────┐ │
│ │ 🔴 Critical        [✓] [ ]         │ │
│ │ ⚠️ Alert           [✓] [ ]         │ │
│ │ 🟡 Warning         [✓] [ ]         │ │
│ │ 🟢 Info            [✓] [ ]         │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ Tipos de Alerta:                        │
│ ┌─────────────────────────────────────┐ │
│ │ [✓] Gás (alarme)                   │ │
│ │ [✓] Fumaça                         │ │
│ │ [✓] Device Offline                 │ │
│ │ [✓] Device Reconectou              │ │
│ │ [✓] Bateria Baixa/Crítica          │ │
│ │ [✓] Temperatura Alta/Baixa         │ │
│ │ [✓] Umidade Fora do Normal         │ │
│ │ [ ] RSSI Fraco                     │ │
│ │ [ ] Memória Baixa                  │ │
│ │ [✓] Relatório Diário (08:00)       │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ Obtenha o token via @BotFather no       │
│ Telegram. O Chat ID pode ser obtido      │
│ enviando /start para @userinfobot.      │
│                                         │
│ [Salvar] [Cancelar]                     │
└─────────────────────────────────────────┘
```

### 17.5 API Endpoints

| Método | Endpoint | Descrição |
|--------|----------|----------|
| `GET` | `/api/config/telegram` | Retorna configuração atual |
| `POST` | `/api/config/telegram` | Salva nova configuração |

**Exemplo GET Response:**
```json
{
  "enabled": true,
  "token": "1234567890:ABCdef...",
  "chat_id": "987654321",
  "poll_interval_ms": 2000,
  "alert_critical": true,
  "alert_alert": true,
  "alert_warning": true,
  "alert_info": true,
  "alert_gas": true,
  "alert_smoke": true,
  "alert_offline": true,
  "alert_reconnect": true,
  "alert_battery": true,
  "alert_temperature": true,
  "alert_humidity": true,
  "alert_rssi": false,
  "alert_heap": false,
  "alert_daily_report": true
}
```

**Exemplo POST Request:**
```json
{
  "enabled": true,
  "token": "1234567890:ABCdef...",
  "chat_id": "987654321",
  "poll_interval_ms": 2000,
  "alert_critical": true,
  "alert_alert": true,
  "alert_warning": true,
  "alert_info": false,
  "alert_gas": true,
  "alert_smoke": true,
  "alert_offline": true,
  "alert_reconnect": false,
  "alert_battery": true,
  "alert_temperature": true,
  "alert_humidity": false,
  "alert_rssi": false,
  "alert_heap": false,
  "alert_daily_report": true
}
```

### 17.6 Persistência

- Configuração salva na EEPROM do ESP32
- Sobrevive a reinicializações
- Mudanças não requerem reinicialização

### 17.7 EEPROM Layout

| Offset | Tamanho | Descrição |
|--------|---------|----------|
| `EEPROM_TELEGRAM_EN_OFFSET` | 1 byte | Habilitado (0/1) |
| `EEPROM_TELEGRAM_TOKEN_OFFSET` | 64 bytes | Bot Token |
| `EEPROM_TELEGRAM_CHATID_OFFSET` | 20 bytes | Chat ID |
| `EEPROM_TELEGRAM_POLL_OFFSET` | 4 bytes | Poll Interval (uint32_t) |
| `EEPROM_TELEGRAM_ALERTS_LVL_OFFSET` | 1 byte | Bitmask de níveis |
| `EEPROM_TELEGRAM_ALERTS_TYPE_OFFSET` | 2 bytes | Bitmask de tipos |

**Bitmask de Níveis (1 byte):**
- Bit 0: CRITICAL (🔴)
- Bit 1: ALERT (⚠️)
- Bit 2: WARNING (🟡)
- Bit 3: INFO (🟢)

**Bitmask de Tipos (2 bytes):**
- Bit 0: Gás (alarme)
- Bit 1: Fumaça
- Bit 2: Device Offline
- Bit 3: Device Reconectou
- Bit 4: Bateria Baixa/Crítica
- Bit 5: Temperatura Alta/Baixa
- Bit 6: Umidade Fora do Normal
- Bit 7: RSSI Fraco
- Bit 8: Memória Baixa
- Bit 9: Relatório Diário

### 17.8 Arquivos Modificados (Dashboard)

| Arquivo | Mudança |
|---------|---------|
| `hub/include/config.h` |Offsets EEPROM para Telegram |
| `hub/src/web_server.cpp` | Endpoints `/api/config/telegram` |
| `hub/include/pages.h` | Seção Telegram no settings + modal |

---

## 18. Referências

- [Universal Telegram Bot Library](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
- [Telegram Bot API](https://core.telegram.org/bots/api)
- [BotFather](https://core.telegram.org/bots#creating-a-new-bot)
- [ESP32 Telegram Tutorial](https://www.wavtron.in/blog/esp32-telegram-bot-notifications)

---

**Versão:** 1.1  
**Data:** 29/08/2026  
**Autor:** AgriSense Team
