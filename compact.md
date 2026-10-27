# compact.md — última posição do trabalho

> **Leia este arquivo antes de iniciar qualquer sessão nova.** Ele registra onde o
> trabalho parou, o que está em andamento, o que foi decidido e o que falta.
> Atualize-o ao final de cada sessão (data + seção "Última posição").

Regras gerais do projeto continuam valendo em `AGENTS.md` (branch `dev`,
política de push, shared como fonte única, regras 1–51).

---

## Última posição

**Data:** 2026-10-26 · **Branch:** `dev` · **Tag/versão:** shared `FW_VERSION v1.2.28`
**Estado:** implementação CONCLUÍDA (não commitada, não pushada). Usuário pediu push para `dev` — **bloqueado: bash indisponível na sessão** (`H.loadWebAssemblyModule`, todos os comandos `git`/`platformio` falham). Falta build + bancada + commits.

### Trabalho concluído nesta sessão (sessão 5 — gating WiFi/TCP do acquiry-bat)

1. **Semântica dos flags definida**: `WIFI_ENABLED` = conectar ao AP (portal/IP); `TCP_ENABLED` = enviar via TCP. `main.cpp:14-16` define `WIFI_ENABLED` a partir de `TCP_ENABLED` (TCP implica WiFi).
2. **Gates `WIFI_ENABLED` → `TCP_ENABLED` flipados** em `nodes/acquiry-bat/src/main.cpp`: `:50` (decl `s_wifi_radio`), `:93` (get_sensor_type), `:177/:183` (espnow_send_cb), `:191` (handle_espnow_recv), `:291` (espnow_send_data), `:437` (espnow_load_gateway_mac), `:487` (setup radio), `:522` (s_wifi_radio.loop), `:624` (force_repair), `:655/698/741/810` (state machine). Gates restantes em `WIFI_ENABLED`: `:14-16`, `:18` (`#error`), `:441`, `:468` — intencionais (conexão).
3. **`setup()` com gating WiFi**: `wifi_setup()` + espera de IP (com deep-sleep em falha) só sob `#ifdef WIFI_ENABLED` (`:441`/`:468`); no `#else` (ESP-NOW puro) portal só pela tecla de config e sem espera de IP.
4. **Verificação IP/RSSI (sem bug)**: hub lê IP do **tail do payload** em `sensor_registry.cpp:305-310` (`payload_len >= expected+4`); node manda `sizeof(payload_int_t)+4` (`:311-317`) — caminho correto (mesmo padrão extender/presence-bat/soil-moisture). `WiFi.RSSI()` sem conexão = 0 não dispara alerta telegram. Node não envia `MSG_HEARTBEAT` ESP-NOW.
5. **Bloco "herdar canal" concluído** (`main.cpp:484-518`, `#else` de `WIFI_ENABLED`): ESP-NOW puro conecta ao AP **só para herdar o canal** (regra 25) — `WiFi.setAutoReconnect(false)` (evita retentativas que pulam de canal), `WiFi.begin()` com creds salvas + espera curta `WIFI_INHERIT_TIMEOUT_MS` (3000 ms, `config.h`), **sem portal e sem exigir IP**: sucesso → loga canal+IP e segue; falha → `WiFi.disconnect(false, false)` (mantém creds — `disconnect()` simples as apaga no core 3.1.2) e segue no canal atual **sem dormir**; sem creds → segue no canal atual. Base: pesquisa `lib-1` (core Arduino 3.1.2: `WL_CONNECTED` = IP; STA reconecta sozinho e pula canal; `WiFi.channel()` = 1–13).
6. **RSSI guard** (`espnow_send_data`, `main.cpp:305`): enviar `WiFi.RSSI()` só se `WL_CONNECTED`, senão 0 — core retorna 31 (fail) desconectado; 0 não dispara alerta telegram.
7. **Verificação IP/RSSI (sem bug)**: hub lê IP do **tail do payload** em `sensor_registry.cpp:305-310` (`payload_len >= expected+4`); node manda `sizeof(payload_int_t)+4` — caminho correto (mesmo padrão extender/presence-bat/soil-moisture). Node não envia `MSG_HEARTBEAT` ESP-NOW.

### Trabalho concluído nesta sessão (sessão 4 — fix leitura acquiry-bat)

1. **`read_acquiry()` corrigido** (`nodes/acquiry-bat/src/main.cpp:156`): lógica de borda com `static old = !val` forçava `state=1` em todo wake após deep sleep (statics zeram no RAM; pino em GND lia 1). Agora semântica nível+hold: HIGH → 1 (reinicia hold); LOW → mantém 1 por `ACQUIRY_HOLD_TIME_MS` e volta a 0. **Validado em bancada** (GPIO4 em GND → `acquiry=0`). Regra 54 adicionada ao AGENTS.md.

### Trabalho concluído nesta sessão (sessão 3 — remoção de `ext_value`)

1. **`payload_int_t` simplificado** (`shared/src/espnow_protocol.h:71-75`, packed **13 bytes**, antes 17):
   - Removido `uint16_t ext_value`. Campo único `value` agora cobre: **`value ≤ 1` = on/off, `value > 1` = leitura do sensor (0-1024)**. `accepts_command` e `label[12]` mantidos. Semântica do espelho `value ≤ 1 → onoff.state` inalterada.
2. **Writers**: `nodes/acquiry-bat/src/main.cpp` — removidos `pl.ext_value = 0` (TCP `get_sensor_payload`) e `pl->ext_value = 0` (ESP-NOW `send_data`). `shared/src/tcp_node_protocol.cpp` `publish_state()` — JSON sem `doc["ext_value"]` (check `len >= sizeof(payload_int_t)` = 13 bytes, automático).
3. **Hub**: `sensor_registry.cpp` case 13 — removida cópia de `ext_value`; `tcp_radio_handler.cpp` — removidos parses `"ext_value"` (case 13 e default); `web_server.cpp` `/api/sensors` default — removido `state["ext_value"]`.
4. **MQTT**: removida a entity genérica `"ext"` (discovery + state + availability) — com `ext_value` fora do protocolo ela duplicaria a entity `"value"`. Entity única `value` (0/1 = on/off, >1 = leitura). **Nota**: HA pode reter entity órfã `*_ext` até remoção manual/re-discovery.
5. **Dashboard**: `pages.h` — removida row "Ext" do fallback genérico de Detalhes (badge/value/label inalterados).
6. **Docs**: `SPEC_TCP.md` (tabela de campos + tipo 13), `SPEC_LORA.md:68`, `SPEC_ESPNOW.md:144`, `AGENTS.md` regra 53 — struct de 13 bytes sem `ext_value`, semântica `value ≤1/>1` documentada.
7. **Quebra de compat 17→13 bytes**: atualizar hub e node acquiry-bat juntos (regra 17); payload 17 bytes de firmware intermediário é rejeitado nos checks `>= sizeof` do hub novo (e vice-versa).

### Trabalho concluído na sessão 2 — protocolo genérico

1. **`payload_int_t` estendido** (`shared/src/espnow_protocol.h:71-76`, packed **17 bytes na época → 13 após sessão 3**, antes 5):
   - `uint16_t value` + `uint8_t accepts_command` + `char label[12]` (`'\0'`-terminado). `ext_value` foi adicionado aqui e **removido na sessão 3** (ver acima).
   - **Rename `eventEnabled` → `accepts_command`** (0 = somente leitura, 1 = aceita comando OnOff do hub). Zero ocorrências de `eventEnabled` no repo (verificado).
   - `label` = texto dinâmico de estado exibido no card do hub (ex.: "aberto"/"fechado") — decisão do usuário: vai no payload de state (muda com o estado), não no registro.

2. **Node `acquiry-bat`**: `get_sensor_payload()` retorna `payload_int_t` (value/ext_value/accepts_command=0/label); labels via `ACQ_LABEL_ON "aberto"` / `ACQ_LABEL_OFF "fechado"` (`config.h:33-36`); ESP-NOW `send_data` preenche label via strncpy+null-term.

3. **Shared TCP** (`tcp_node_protocol.cpp:398-421`): branch nova em `publish_state()` — se o payload for `payload_int_t` (≥17 bytes), envia `value`/`ext_value`/`state`/`label` no JSON `/node/state`; nodes lamp/switch (1 byte `payload_onoff_t`) inalterados.

4. **Hub**:
   - `sensor_registry.cpp:267-288`: case 13 copia ext_value+label; default genérico sanitiza label após memcpy.
   - `tcp_radio_handler.cpp:427-461`: case 13 + default — `label` opcional no JSON, casts `ext_value` uint8→uint16.
   - `web_server.cpp:413-416` e `:430-436`: `/api/sensors` emite `label` (só se `label[0]`) nos cases 13 e default. **Lacuna menor:** case 13 não emite `ext_value` (cosmético; node hoje envia 0).
   - `pages.h:652-657`: badge do card — label (escHtml) prioriza sobre ATIVA/INATIVA e sobre value; `:882` Detalhes 13 — label prioriza; `:890-895` genérico — linha "Estado" com label.
   - Docs: `SPEC_LORA.md:68`, `SPEC_ESPNOW.md:144`, `SPEC_TCP.md:91+` com struct novo.

5. **Decisões/limites**: MQTT intocado (state topic incompatível se mudasse; label é feature dashboard/API — expor no HA = follow-up). `accepts_command` ainda não altera semântica de comando do hub (follow-up). `label` só gravado em case 13 + default (evita clobber da união, ex.: temp_hum).

6. **Quebra de compat 5→17 bytes**: atualizar hub e node acquiry-bat juntos (regra 17). Firmware antigo × hub novo = payload rejeitado nos checks `>= sizeof`.

### Trabalho concluído nesta sessão (sessão 1 — node + hub + OTA)

1. **Node `nodes/acquiry-bat`** (sensor genérico de aquisição, `SENSOR_TYPE_ACQUIRY = 13`, ESP-NOW **ou** TCP exclusivos) — revisão + correções:
   - **Causa raiz do não-registro em ESP-NOW removida**: `#ifndef WIFI_ENABLED → force_config_portal = true` forçava o portal WiFiManager em todo boot (node nunca entrava no AP → canal errado → PAIR_REQUEST nunca chegava). Agora portal só: tecla, sem SSID salvo, ou falha de conexão.
   - Exclusividade WiFi/TCP × ESP-NOW agora é `#error` (era `#undef` silencioso).
   - `firmware_version` do pair request derivado de `FW_VERSION` (era `0x000A000B` hardcoded).
   - Bateria via ADC (`BATTERY_ADC_PIN -1` = desabilitado por padrão; sem simulação de drift).
   - `delay(1000)`/busy-spin do `STATE_WAIT_ACK` → `delay(50)` por iteração.
   - `platformio.ini`: sem `-I` manual; novo env `[env:esp8266_espnow]` (só `-DESPNOW_ENABLED`).
   - Comentário stale `PIR_HOLD_TIME_MS` corrigido.

2. **Hub** — suporte ao tipo 13 + robustez:
   - `sensor_registry`: payload `payload_int_t.value` → `state.onoff.state`; nomes "Aquisição"/"acquiry".
   - `tcp_radio_handler::handle_state`: aceita `state` bool/uint8 (como o node TCP envia).
   - MQTT: `binary_sensor` entity suffix `acq`, read-only (sem command_topic/optimistic), discovery/state/availability.
   - `/api/sensors`: emite `state`; dashboard: badge ATIVA/INATIVA, ícone 📡, linha em Detalhes.
   - Docs: tabelas de tipo 12/13 em `SPEC_ESPNOW.md`/`SPEC_LORA.md`/`SPEC_TCP.md`.
   - **Fallback genérico em 8 switches por tipo** (`// fallback genérico: tipos novos nunca são descartados`): qualquer tipo desconhecido cai em `value`/`ext_value` (+ espelho `onoff.state` quando 0/1). Nenhum caminho de registro rejeita tipo novo (verificado: sem whitelist).
   - **OTA web `/update` corrigido**: causa do "erro no final do upload" era `delay(500)`+`ESP.restart()` dentro do handler async (morreria antes de drenar a resposta). Agora: resposta com `Connection: close`, `Update.end(true)`, restart agendado via `web_server_request_restart(1500)` no `web_server_loop()`, diagnóstico `Update.errorString()` em begin/write/end (console + `/logs`).
   - OTA capacity visível: boot log `[OTA] free_sketch=%u sketch=%u`; `/api/info` com `free_sketch`/`sketch_size`/`ota_port`.

3. **`compact.md` criado** e `AGENTS.md` com a regra de lê-lo/atualizá-lo por sessão.

### Pendências (próxima sessão)

1. **Rodar builds** (bash indisponível nas sessões 2–3): `cd hub && platformio run -e hub_32 && platformio run -e hub_32_ota`; `cd nodes/acquiry-bat && platformio run -e esp8266 && platformio run -e esp8266_espnow`; `cd tests/unit && platformio test`. Struct final = **13 bytes**; validar que tudo compila.
2. **Commits pendentes** (não executáveis sem bash): commit local no submodule `shared` (branch dev) com o struct novo, depois commit no homeware bumpando o SHA. Sem push em ambos. Nada commitado ainda.
3. **Bancada**: (a) OTA web no hub — upload deve dar 200 + browser sem erro + restart ~1,5s; conferir `/logs` se falhar. (b) espota — se ainda falhar: IP via `scan.py`, firewall, `OTA: erro N` no serial. (c) `acquiry-bat` ESP-NOW: flashar env `esp8266_espnow`, primeiro boot abre portal → configurar → deve parear; card deve mostrar "aberto"/"fechado". (d) Hub com tipo desconhecido (ex.: 99) → genérico com label. (e) Testar TCP do acquiry-bat (JSON `/node/state` com value/label) e conferir no HA se a entity órfã `*_ext` de tipos genéricos precisa de limpeza.
4. **Follow-ups conhecidos** (fora do escopo desta sessão):
   - `/api/restart` e `/api/config/mode` ainda fazem `delay`+`ESP.restart()` no handler async — migrar para `web_server_request_restart()`.
   - Hub ainda ignora `accepts_command` — implementar liberação de comando OnOff quando = 1 (web/telegram/MQTT).
   - Se quiser label no HA: expor como atributo/entity field no MQTT (state topic intocado por compat).
5. Após validação: commit local no `dev` (push só com autorização do usuário). Considerar issues para os follow-ups.

---

## Mapa rápido do que foi tocado nesta sessão

| Arquivo | Mudança |
|---|---|
| `shared/src/espnow_protocol.h` | `payload_int_t` final **13 bytes**: `value` (0/1 = on/off, >1 = leitura) + `accepts_command` (rename de `eventEnabled`) + `label[12]`; `ext_value` removido (sessão 3) |
| `shared/src/tcp_node_protocol.cpp` | `publish_state()` branch `payload_int_t` → JSON com value/state/label |
| `nodes/acquiry-bat/src/main.cpp` | `get_sensor_payload()` + ESP-NOW `send_data` com label |
| `nodes/acquiry-bat/include/config.h` | `ACQ_LABEL_ON/OFF` ("aberto"/"fechado") |
| `hub/src/sensor_registry.cpp` | tipo 13: label; default: sanitiza label |
| `hub/src/tcp_radio_handler.cpp` | 13 + default: label opcional; parses ext_value removidos |
| `hub/src/web_server.cpp` | `/api/sensors` emite label (13 + default); OTA corrigido |
| `hub/src/mqtt_client.cpp` | discovery/state/availability `binary_sensor acq`; entity genérica `ext` removida (sessão 3) |
| `hub/include/pages.h` | badge/ Detalhes: label prioriza; fallback genérico |
| `hub/SPEC_*.md` | tabelas de tipos 12/13 com struct novo |
| `nodes/acquiry-bat/*` | correções de modo ESP-NOW/TCP |
| `compact.md` | este arquivo |
