# AgriSense IoT — Roadmap

## Pesquisa de Mercado — Funcionalidades Requisitadas (Agosto 2026)

### Resumo Executivo

Análise comparativa entre o que o mercado de IoT/Agronegócio demanda em 2025-2026 e o que o AgriSense já oferece. Fontes: ESPHome, Tasmota, Home Assistant community, fóruns ESP32, papers de IoT agrícola.

---

### Comparativo: Mercado vs. AgriSense

| Funcionalidade | Mercado | AgriSense | Gap | Prioridade |
|---|---|---|---|---|
| **Controle por voz** | Google Home, Alexa, HomeKit — os 3 | ✅ Alexa (Espalexa) nos nodes; **via HA**: Google Home, HomeKit, Matter | ⚠️ Google Home/HomeKit via HA (já funciona) | 🟢 Baixa (via HA) |
| **Notificações push** | Telegram, WhatsApp, Email, Push | ❌ Nenhum suporte | ❌ Nenhum canal de alerta | 🔴 Alta |
| **Histórico de dados** | Charts temporais, export CSV, tendências | ✅ **Via HA**: Long-Term DB, Grafana, Energy Dashboard, export CSV | ✅ Disponível via HA | 🟢 Baixa (via HA) |
| **Deep Sleep / Energia** | Bateria, solar, MPPT, energy harvesting | ✅ soil-moisture e presence-bat | ⚠️ Sem monitoring de bateria, sem solar | 🟡 Média |
| **Home Assistant (avançado)** | MQTT Discovery, availability, device triggers | ✅ MQTT Discovery implementado | ⚠️ Sem availability, sem device triggers | 🟠 Média |
| **OTA remoto via hub** | Update em massa, rollback, versionamento | ✅ OTA local via browser | ❌ Sem OTA via hub, sem rollback | 🟡 Média |
| **Protocolo unificado** | Matter sobre Thread/WiFi | ❌ Não suportado; **via HA**: ✅ Matter nativo (HA 2023.x+) | ⚠️ Matter via HA (já funciona) | 🟢 Baixa (via HA) |
| **Regras / Automação** | Condições, timer, cenas, fallback | ❌ Tudo manual; **via HA**: ✅ Automações nativas | ⚠️ Sem regras no AgriSense; HA já tem | 🟡 Baixa (via HA) |
| **Multi-hub / Mesh** | Multi-gateway, mesh networking | ✅ ESP-NOW + LoRa + TCP | ⚠️ Sem auto-discovery entre hubs | 🟡 Baixa |
| **Dashboard avançado** | Gráficos interativos, Figma-like | ✅ Dashboard responsivo (HTML/JS); **via HA**: ✅ Grafana, ApexCharts, Energy Dashboard | ⚠️ Sem charts no AgriSense; HA já tem | 🟡 Baixa (via HA) |
| **Integrações cloud** | AWS IoT, Azure, Firebase, Blynk | ❌ Nenhuma | ❌ Sem cloud (by design — local-first) | ⚪ Opcional |
| **Segurança** | TLS, certs, OAuth, rate limiting | ⚠️ HTTP plain, sem auth | ❌ Sem TLS nos nodes TCP | 🟠 Média |
| **Documentação** | README, wiki, exemplos, vídeo | ✅ SPEC.md, TECHNICAL.md | ⚠️ Sem exemplos, sem vídeo | 🟡 Baixa |
| **Testes automatizados** | CI/CD, unit tests, integration tests | ✅ tests/ (83 arquivos detectados) | ⚠️ Sem CI/CD configurado | 🟡 Baixa |
| **Multi-usuário** | Permissões, ACLs, shared access | ❌ Single-user | ❌ Sem controle de acesso | ⚪ Opcional |

---

### Análise Detalhada por Área

#### 1. 🔊 Controle por Voz Multi-Plataforma
**Demanda:** #1 que usuários de smart home pedem. Matter é o padrão emergente em 2026.
**Estado:** Alexa funciona via Espalexa nos nodes lamp e switch.
**Opções:**
- **HomeKit** via lib `HomeSpan` — complexidade média, funciona em ESP32/ESP8266
- **Google Home** — indireto via Home Assistant (já funciona se HA estiver configurado)
- **Matter** — requer `esp-matter` SDK da Espressif, complexidade alta, suporta ESP32/ESP32-C3/ESP32-H2
**Recomendação:** Priorizar Google Home via HA (já funciona indiretamente). Matter como objetivo de longo prazo quando o SDK amadurecer.

#### 2. 📱 Notificações Push / Alertas Remotos
**Demanda:** Alertas de alarme (gás, presença), device offline, resumo diário de sensores.
**Estado no AgriSense:** Nenhum suporte — depende 100% do dashboard local ou MQTT.
**No Home Assistant:** ✅ **Já disponível** — HA possui integração Telegram nativa (notificações + comandos interativos).
**Opções:**
- **Telegram no Hub (direto)** — HTTP simples, funciona sem HA, baixa latência
- **Telegram via HA** — ✅ Já funciona, mas requer HA rodando + configuração YAML
- **WhatsApp Business API** — requer provedor (Twilio, Evolution API), custo mensal
- **Email (SMTP)** — funciona, mas lento e menos conveniente
- **Push notification (Firebase)** — requer conta Google, mais complexo
**Recomendação:** Telegram no hub (direto) para alertas críticos (gás, offline) + Telegram via HA para automações. Dual canal para redundância.

#### 3. 📊 Histórico de Dados / Gráficos
**Demanda:** Charts temporais para decisões de irrigação, monitoramento de clima, tendências.
**Estado no AgriSense:** Dados são pontuais — `GET /api/state` retorna valor atual.
**No Home Assistant:** ✅ **Já disponível** — HA possui Long-Term Database, Grafana, Energy Dashboard, Mini Graph Card, ApexCharts.
**Opções:**
- **SD card no hub** — armazenamento local, ~32GB, sem custo recorrente
- **SPIFFS/LittleFS no hub** — limitado a ~1-2MB, suficiente para dados comprimidos
- **Banco local (SQLite via server Python)** — mais robusto, requer server rodando
- **InfluxDB + Grafana** — solução profissional, mas pesada para ESP32
**Recomendação:** SD card no hub para dados brutos + endpoint `GET /api/sensors/export?from=&to=` em CSV. Gráficos via Grafana ( opcional, rodando no server Python).

#### 4. 🔋 Deep Sleep / Economia de Energia
**Demanda:** Nodes alimentados por bateria/solar com deep sleep agressivo.
**Estado:** Implementado em `soil-moisture` e `presence-bat`.
**Opções:**
- **Monitoring de bateria** — ADC no hub, alerta quando < 20%
- **Suporte solar** — MPPT simples (CN3791), energy harvesting
- **Deep sleep adaptativo** — aumentar intervalo quando bateria baixa
**Recomendação:** Adicionar monitoring de bateria via ADC nos nodes que já têm bateria. Solar como opção futura.

#### 5. 🏠 Integração Home Assistant (Avançado)
**Demanda:** MQTT Discovery automático, availability, device triggers, firmware update status.
**Estado:** MQTT Discovery implementado, publica entidades automaticamente.
**Opções:**
- **Availability topics** — publicar `online`/`offline` em `homeassistant/.../availability` ✅ IMPLEMENTADO
- **Device triggers** — eventos como botão pressionado, detecção de movimento
- **Firmware update** — tópico `homeassistant/.../firmware_version`
**Recomendação:** Availability já implementado. Device triggers como próximo passo.

#### 6. 🔄 OTA Remoto via Hub
**Demanda:** Atualização em massa de nodes via hub, rollback automático.
**Estado:** OTA local via browser em cada node.
**Opções:**
- **HTTP proxy** — hub serve firmware, node baixa via HTTP (já parcialmente implementado)
- **ESP-NOW OTA** — enviar firmware via ESP-NOW (limitado por MTU ~250 bytes)
- **TCP OTA** — node TCP baixa firmware do hub via HTTP (mais robusto)
**Recomendação:** Priorizar HTTP proxy no hub + rollback automático via watchdog.

#### 7. 🏗️ Matter / Protocolo Unificado
**Demanda:** Padrão emergente que unifica Google Home, Alexa, HomeKit.
**Estado no AgriSense:** Não suportado diretamente.
**No Home Assistant:** ✅ **Já suportado** — HA 2023.x+ tem integração Matter nativa. AgriSense pode se beneficiar via HA.
**Opções:**
- **Via HA** — ✅ Matter já funciona no HA, AgriSense conecta via MQTT
- **esp-matter SDK** — requer ESP32, ESP32-C3, ou ESP32-H2 (futuro)
**Recomendação:** Usar HA como bridge Matter. Não implementar Matter diretamente no firmware agora.

#### 8. ⚙️ Regras / Automação
**Demanda:** Condições (se temp > 30, ligar irrigação), timer, cenas, fallback.
**Estado:** Tudo manual — usuário precisa de HA para automações.
**Opções:**
- **Regras no hub** — engine simples via `POST /api/rules` com JSON
- **Regras nos nodes** — lógica embarcada (limitado por memória)
- **Regras no HA** — Automações do HA (já funciona)
**Recomendação:** Implementar engine de regras simples no hub para cenários offline (sem HA).

#### 9. 📊 Dashboard Avançado
**Demanda:** Gráficos interativos, responsivo mobile, dark mode.
**Estado:** Dashboard funcional mas básico (HTML/JS inline).
**Opções:**
- **Chart.js** — leve, funciona em ESP8266 com chunked response
- **Apache ECharts** — mais pesado, mas mais funcional
- **Serve do hub** — dashboard unificado no hub para todos os nodes
**Recomendação:** Adicionar Chart.js para gráficos temporais + responsividade mobile.

#### 10. 🔒 Segurança
**Demanda:** TLS, autenticação, rate limiting.
**Estado:** HTTP plain nos nodes TCP, sem auth.
**Opções:**
- **HTTPS no hub** — Let's Encrypt + ESP32 (funciona, mas requer cert management)
- **API key** — header `X-API-Key` simples
- **mTLS** — complexo para ESP8266
**Recomendação:** API key como mínimo. HTTPS no hub como opcional (requer cert management).

---

### Recomendações Priorizadas

#### Fase 1 — Quick Wins (1-2 semanas cada)
1. **Notificações Telegram** — alertas de alarme, device offline, resumo diário
   - 📋 **SPEC:** `hub/SPEC_telegram.md`
   - ✅ **Dashboard:** Configuração via `/settings` (token, chat_id, enable/disable)
2. **Availability MQTT** — ✅ JÁ IMPLEMENTADO
3. **Google Home via HA** — ✅ **JÁ DISPONÍVEL** no HA (Nabu Casa ou manual)
4. **Dashboard charts** — ✅ **JÁ DISPONÍVEL** no HA (Grafana, ApexCards, Energy Dashboard)

#### Fase 2 — Médio Prazo (2-4 semanas cada)
5. **Histórico de dados** — ✅ **Usar HA** (Long-Term DB, Grafana, Energy Dashboard) — não implementar no hub
6. **OTA remoto via hub** — HTTP proxy + rollback automático (apenas via dashboard, sem Telegram)
7. **Regras simples no hub** — engine de automação offline
8. **Segurança básica** — API key nos endpoints HTTP

#### Fase 3 — Longo Prazo (1-3 meses)
9. **HomeKit** — lib HomeSpan nos nodes lamp/switch
10. **Device triggers MQTT** — eventos de botão/presença no HA
11. **Multi-hub discovery** — auto-discovery entre hubs na LAN
12. **Deep sleep adaptativo** — monitoramento de bateria + alertas

#### Futuro Distante (6+ meses)
13. **Matter** — quando esp-matter SDK amadurecer
14. **Multi-usuário** — permissões e ACLs
15. **Cloud opcional** — integração com AWS IoT / Firebase

---

### Benchmark: AgriSense vs. Projetos Comparáveis

| Projeto | Tipo | Comunicação | Dashboard | HA Integration | OTA | Custo |
|---|---|---|---|---|---|---|
| **AgriSense** | Hub + Nodes | ESP-NOW + LoRa + TCP | ✅ Local | ✅ MQTT Discovery | ✅ Local | ~$5-15/node |
| **ESPHome** | Firmware | WiFi | ❌ (usa HA) | ✅ Nativo | ✅ Via HA | ~$5-10/node |
| **Tasmota** | Firmware | WiFi | ✅ Local | ✅ MQTT | ✅ Local | ~$3-8/node |
| **OpenMQTTGateway** | Gateway | Multi-protocol | ❌ | ✅ Nativo | ✅ Local | ~$15-30/gateway |
| **ESP-NOW Mesh** | DIY | ESP-NOW mesh | ❌ | ⚠️ Manual | ❌ | ~$3-5/node |

**Vantagem competitiva do AgriSense:**
- Multi-protocol (ESP-NOW + LoRa + TCP) — nenhum outro projeto oferece isso
- Hub centralizado com dashboard local — não depende de cloud
- MQTT Discovery automático — integração plug-and-play com HA
- Custos baixos — nodes ESP8266 a ~$3-5 cada

**Áreas para melhorar:**
- Segurança (TLS, auth)
- Histórico de dados (no AgriSense; HA já tem)
- Notificações push
- Matter (no AgriSense; HA já tem)

---

## Estimativa de Memória — Integração Telegram no Hub ESP32

### Contexto

O hub roda no **ESP32** (Heltec W32 LoRa) com as seguintes funcionalidades ativas:
- ESP-NOW (recebimento de dados de nodes)
- LoRa (comunicação de longa distância)
- TCP (nodes WiFi)
- AsyncWebServer (dashboard local)
- MQTT Client (integração Home Assistant)
- Display OLED (Heltec)
- Sensor Registry (EEPROM)

**ESP32 WROOM:** 520KB SRAM total, ~320KB heap disponível

### Composição de Memória Atual (Estimada)

| Componente | RAM Estática | RAM Dinâmica (Heap) | Notas |
|---|---|---|---|
| WiFi + TCP/IP stack | ~8KB | ~30-50KB | LWIP, buffers de conexão |
| ESP-NOW | ~2KB | ~5-10KB | Canal 1, callbacks |
| LoRa (SX1276) | ~1KB | ~2-5KB | SPI, buffers TX/RX |
| AsyncWebServer | ~5KB | ~15-25KB | Conexões assíncronas, chunked |
| MQTT Client (PubSubClient) | ~2KB | ~5-10KB | Buffers de subscribe/publish |
| ArduinoJson | ~1KB | ~8-15KB | Documentos JSON dinâmicos |
| Sensor Registry | ~3KB | ~2-5KB | EEPROM, array de sensores |
| Display OLED | ~1KB | ~1-2KB | Buffer do framebuffer |
| WiFiManager | ~2KB | ~5-10KB | Portal captive, config |
| OTA (ArduinoOTA) | ~1KB | ~2-5KB | HTTP server, buffers |
| **TOTAL ESTIMADO** | **~26KB** | **~75-135KB** | |
| **Heap Livre Estimado** | | **~185-245KB** | Após todas as libs |

> **Nota:** Valores baseados em medições típicas de projetos ESP32 com libs similares. O `GET /api/state` já reporta `free_heap` para validação em campo.

### Consumo Estimado: UniversalTelegramBot + WiFiClientSecure

| Componente | RAM Estática | RAM Dinâmica (Heap) | Notas |
|---|---|---|---|
| **UniversalTelegramBot** | ~2KB | ~3-5KB | Parsing de mensagens, buffers |
| **WiFiClientSecure (mbedTLS)** | ~4KB | ~20-30KB | TLS handshake, certificados, buffers de sessão |
| **Código do bot (comandos)** | ~1KB | ~2-3KB | String handling, dispatch |
| **TOTAL TELEGRAM** | **~7KB** | **~25-38KB** | |

### Análise de Impacto

| Cenário | Heap Livre Atual | + Telegram | Heap Restante | Viável? |
|---|---|---|---|---|
| **Mínimo** (poucos nodes, sem LoRa) | ~220KB | +25KB | ~195KB | ✅ Sim |
| **Típico** (5-10 nodes, LoRa ativo) | ~185KB | +30KB | ~155KB | ✅ Sim |
| **Máximo** (10+ nodes, web server ativo) | ~150KB | +38KB | ~112KB | ✅ Sim (apertado) |
| **Crítico** (muitas conexões TCP) | ~120KB | +38KB | ~82KB | ⚠️ Risco de fragmentação |

### Riscos e Mitigações

| Risco | Severidade | Mitigação |
|---|---|---|
| **Fragmentação de heap** | 🟠 Média | Usar `longPoll` (60s) para reduzir requests; liberar buffers após uso |
| **TLS handshake pesado** | 🟠 Média | Cache de sessão TLS; não reconectar a cada poll |
| **Múltiplas conexões simultâneas** | 🟡 Baixa | Telegram bot usa 1 conexão por vez (polling sequencial) |
| **Crash por OOM** | 🟠 Média | Monitorar `ESP.getFreeHeap()`; desabilitar Telegram se < 50KB |
| **Latência do polling** | 🟡 Baixa | Intervalo de 1-2s é suficiente; não impacta loop principal |

### Otimizações Propostas

1. **Long Polling** (60s) — reduz requests de ~1/s para ~1/min, economiza RAM e bateria
2. **TLS Session Reuse** — mbedTLS reutiliza sessão, reduz handshake de ~30KB para ~5KB
3. **Buffer Estático** — usar `char[]` fixo em vez de `String` para respostas
4. ** PROGMEM** — armazenar textos de comandos em flash, não em RAM
5. **Desabilitação dinâmica** — se `free_heap < 50KB`, pausar polling do Telegram

### Configuração do Build

```ini
; Adicionar ao platformio.ini do hub
lib_deps = 
    bblanchon/ArduinoJson@^7.2.1
    knolleary/PubSubClient@^2.8
    ESP32Async/AsyncTCP@^3.3.2
    ESP32Async/ESPAsyncWebServer@^3.6.0
    witnessmenow/UniversalTelegramBot@^1.3.0  ; NOVO

build_flags = 
    -D TELEGRAM_ENABLED
    -D BOT_TOKEN=\"SEU_TOKEN_AQUI\"
    -D CHAT_ID=\"SEU_CHAT_ID\"
    -D TELEGRAM_POLL_INTERVAL=2000  ; 2 segundos
    -D TELEGRAM_LONG_POLL=60         ; 60 segundos
```

### Conclusão

**✅ Cabe no ESP32 do hub.**

Com a configuração atual (ESP-NOW + LoRa + TCP + AsyncWeb + MQTT), o hub fica com **~150-220KB de heap livre**. Adicionar Telegram consome **~25-38KB adicionais**, resultando em **~112-195KB restantes** — suficiente para operação estável.

**Recomendação:** Implementar com as otimizações acima (long poll, TLS reuse, desabilitação dinâmica). O risco é baixo e o benefício (controle remoto + notificações) é alto.

---

## Futuro

### Hub IP fixo no node TCP
- **Problema**: `TcpNodeProtocol` descobre o hub via UDP broadcast (primeiro que responder ganha). Se 2 hubs estiverem na mesma rede, o node registra em um apenas.
- **Solução**: campo "Hub IP" no WiFiManager (salvo na EEPROM). Se configurado, o node pula o UDP discover e registra direto no hub configurado. Se não, usa o comportamento atual.
- **Impacto**: `tcp_node_protocol.cpp`, WiFiManager dos nodes TCP (lamp, etc.)

### Cobertura por tecnologia
- **LoRa**: localidades distantes, alcance longo, baixa taxa de dados
- **ESP-NOW**: pontos sem alcance do router, comunicação direta node↔hub sem WiFi
- **TCP/WiFi**: nodes próximos ao router, maior throughput, dashboards via HTTP
