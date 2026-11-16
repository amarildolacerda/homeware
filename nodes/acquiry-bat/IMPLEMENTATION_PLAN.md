# Plano de Implementação — Dual Power Mode (acquiry-bat)

Baseado em `batery_modes.md` e nas regras do projeto (AGENTS.md). Objetivo: adaptar o firmware para detectar a fonte de alimentação no boot e operar em Battery Mode (deep sleep) ou Powered Mode (WiFi contínuo + dashboard/OTA).

## Escopo
- Node: `nodes/acquiry-bat`
- Shared: `shared/` (sem mudanças de protocolo esperadas)
- Hub: sem mudanças (já aceita ESP-NOW do node)

## Fases

### Fase 1 — Detecção de fonte de alimentação (boot)
- Adicionar `detect_power_source()` chamada em `setup()` antes de qualquer init.
- Preferencial: monitorar VBUS via GPIO (definir `POWER_DETECT_PIN`).
- Fallback: ADC de tensão de entrada.
- Enum `PowerMode { BATTERY_MODE, POWERED_MODE }` em `config.h` ou header local.
- Persistir modo detectado? Não — re-detectar a cada boot (deep sleep reinicia).

### Fase 2 — Battery Mode (comportamento atual)
- Extrair o fluxo atual (init mínimo, leitura, ESP-NOW send, deep sleep) em `start_battery_mode()`.
- Garantir deep sleep < 20µA, wake < 240s.
- Sem WiFi contínuo, sem web server, sem OTA.

### Fase 3 — Powered Mode
- Iniciar WiFi contínuo (`mywifi_begin` + `mywifi_loop`).
- Habilitar web server (dashboard padrão, regra 28) com endpoints obrigatórios (regra 29).
- Habilitar OTA (ArduinoOTA / common_ota).
- Telnet/console (regra 30).
- Manter ESP-NOW ativo para receber comandos do hub? Opcional — definir.

### Fase 4 — Integração e gating
- Gates `#ifdef`/runtime por modo para evitar inicializar hardware não usado.
- `loop()` despacha para `battery_loop()` ou `powered_loop()` conforme modo.
- Deep sleep apenas no Battery Mode.

### Fase 5 — Testes
- Teste de detecção (VBUS presente/ausente).
- Teste de consumo em deep sleep.
- Teste de dashboard/OTA no Powered Mode.

## Riscos
- Pino de detecção de VBUS pode não existir no hardware alvo → fallback ADC.
- Deep sleep no ESP8266 vs ESP32: APIs diferentes.
- WiFi contínuo aumenta consumo — apenas Powered Mode.

## Arquivos a alterar
- `nodes/acquiry-bat/src/main.cpp` — setup/loop, detect_power_source, dispatch.
- `nodes/acquiry-bat/include/config.h` — pins, timeouts, POWER_DETECT_PIN.
- Novo: `nodes/acquiry-bat/src/power_mode.h/.cpp` (opcional, se separar).
- `nodes/acquiry-bat/platformio.ini` — flags se necessário.

## Próximo passo
Implementar Fase 1 (detecção) e validar em bancada antes de avançar.