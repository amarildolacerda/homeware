# compact.md — última posição do trabalho

> **Leia este arquivo antes de iniciar qualquer sessão nova.** Ele registra onde o
> trabalho parou, o que está em andamento, o que foi decidido e o que falta.
> Atualize-o ao final de cada sessão (data + seção "Última posição").

Regras gerais do projeto continuam valendo em `AGENTS.md` (branch `dev`,
política de push, shared como fonte única, regras 1–57).

---

## Última posição

**Data:** 2026-11-25 · **Branch:** `dev` · **Tag/versão:** inalterada (shared `FW_VERSION v1.2.29`)
**Estado:** `setup.sh` corrigido e rodou até o fim (EXIT=0). `.venv` estava quebrado (shebang `/mnt/c/git/homeware/...`, `bin/python` de 0 bytes) e foi recriado. Sem push.

### Trabalho concluído nesta sessão (sessão 13 — setup.sh não rodava)

1. **Causa**: Ubuntu 24.04 / Python 3.12 com PEP 668 (*externally-managed-environment*). `python3 -m pip install --user -U platformio` falhou duas vezes; como a segunda tentativa vinha depois de `||`, o `set -e` abortou o script antes de chegar ao `pio --version`.
2. **`setup.sh`**: removido `pip --user`. Ordem agora: `pio` no PATH → `~/.platformio/penv/bin/pio` → criar/reparar `.venv` e instalar PlatformIO dentro dele. Bloco antigo que só procurava o bin do user-site foi trocado por fallback para `.venv/bin` + mensagem de PATH persistente.
3. **`setup_venv.sh`**: passou a recriar o venv quando ele está ausente **ou quebrado** — validação por saída não-vazia de `python -m pip --version` (um arquivo executável de 0 bytes responde exit 0 sem saída, então `-x` sozinho não detecta).
4. **`AGENTS.md`**: regra 57 registrada (PEP 668 / setup sem pip de sistema).
5. **Validação**: `bash setup.sh` → EXIT=0, sem erros de externally-managed; `pio --version` = 6.2.0 (via `~/.platformio/penv/bin`); `.venv` recriado com deps de `requirements.txt`.
6. **Sequência do usuário**: `cd hub && ./flash.sh -o 192.168.1.14` falhou com `platformio: comando não encontrado` — o `export PATH` dentro do `setup.sh` não persiste no shell. Criado `pio_env.sh` (raiz) que resolve o executável; scripts `flash.sh`, `hub/flash.sh`, `hub/build.sh`, `hub/monitor.sh`, `hub/erase.sh` agora sourceiam e chamam `"$PIO"`. Validado com `PATH=/usr/bin:/bin`: resolve para `~/.platformio/penv/bin/pio` 6.2.0. Sem push; OTA não executado por mim.

### Trabalho concluído nesta sessão (sessão 12 — Telnet em Powered Mode)

1. Telnet usa `WiFiServer` na porta 23; `console.begin()` inicia o listener e `loop_powered()` aceita clientes/processa comandos.
2. `console.set_banner(DEVICE_NAME)` identifica a sessão remota; `setup_powered()` mostra o IP e a porta quando WiFi está conectado.
3. Build `esp32_c3` passou. A tentativa de upload falhou porque `/dev/ttyACM0` estava ocupado; nenhum processo foi encerrado.

### Trabalho concluído nesta sessão (sessão 11 — dashboard em Powered Mode)

1. Verificado que `setup()` chama `setup_powered()` quando o modo detectado é `POWERED_MODE`.
2. `setup_powered()` já chama `dashboard_begin()`; isso registra `/` e `/docs` e inicia o servidor na porta 80. `loop_powered()` chama `dashboard_loop()` em cada iteração.
3. Nenhuma alteração de código necessária para ativar o dashboard. Acessar `http://<IP-do-node>/` após conectar ao WiFi.

### Trabalho concluído nesta sessão (sessão 10 — avisos de defines duplicados)

1. Removido `-DWIFI_ENABLED` dos envs TCP: `main.cpp` já ativa WiFi quando `TCP_ENABLED` está definido.
2. No C3, mantido somente `WIFI_CHANNEL=4` (valor efetivo anterior; o define `=1` duplicado não era usado no código).
3. Removidas as definições locais duplicadas `WIFI_CONFIG_PORTAL_SSID/PASS` de `config.h`; o valor efetivo já era o default do `shared_config.h`.
4. Validação: builds `esp32_c3`, `esp8266_tcp` e `esp8266_ota` passaram sem avisos de macro redefinida. Ainda há warnings independentes de APIs deprecated e variáveis unused.

### Trabalho concluído nesta sessão (sessão 9 — reinício do acquiry-bat ESP32-C3)

1. **Causa raiz do assert**: stack da placa mostrou `ConsoleOutput::begin()` → `NetworkServer::begin()` → `lwip_socket()` → `sys_mutex_lock()`, antes de `WiFi.mode()` inicializar `Network.begin()`. Movido `console.begin()` para depois de `WiFi.mode(WIFI_STA)` em `nodes/acquiry-bat/src/main.cpp`.
2. **Modo de alimentação**: por decisão do usuário, `platformio.ini` define `-DPOWERED_USB=1` somente em `env:esp32_c3`; `detect_power_source()` consome a flag e retorna `POWERED_MODE`. Outros alvos mantêm a detecção atual.
3. **Robustez adicional**: expiração do portal WiFi em Powered Mode não reinicia e deixa a reconexão para `powered_maintain_wifi()`.
4. **Validação**: `pio run -e esp32_c3` passou; firmware gravado/verificado em `/dev/ttyACM0`. A captura posterior não encontrou linhas de monitor; build ainda avisa redefinição de `WIFI_CHANNEL`.
5. **Próximo passo**: abrir monitor a 115200 antes de resetar a placa e confirmar o banner/boot. O firmware não imprime telemetria continuamente.

### Trabalho concluído nesta sessão (sessão 8 — botão ligar/desligar do hub não chega ao lamp)

1. **Causa raiz** (`shared/src/selectable_node_protocol.*`): `NodeProtocol::callbacks` é membro por instância. O `lamp` (envs `esp8266_espnow`/`esp8266_ota` têm `RADIO_SELECT_ENABLED`) usa `s_radio.callbacks = {...}`, que ficava só no `SelectableNodeProtocol`; `m_espnow` e `m_tcp` continuavam com `nullptr`. Resultado: `on_command` nunca chamado (botão do hub inócuo), `get_sensor_payload` nunca chamado (estado nunca atualiza no hub) e `get_sensor_type` nulo → registra como `SENSOR_TYPE_ONOFF` (TCP) / 0 (ESP-NOW).
2. **Fix**: novo `SelectableNodeProtocol::sync_callbacks()` copiando `callbacks` para `m_espnow` e `m_tcp`, chamado em `set_mode()`, `begin()` e `loop()` (cobre atribuição depois do `begin`).
3. **Defensivo**: `NodeCallbacks callbacks = {}` em `node_protocol.h` — callback ausente é `nullptr` (já checado), não ponteiro indefinido.
4. **Validação**: `pio run -e esp8266_espnow` no `nodes/lamp` SUCCESS. `tests/unit` (native) **não compila por breakage pré-existente** (`platform.h` inclui `ESP8266WiFi.h` no env native, sem mock) — não relacionado a esta mudança.
5. **Bancada pendente**: reflashar o lamp, conferir que o tipo passa a 9 (Lâmpada) no card do hub e que `[lamp] Command received` / `TCP cmd: ON` aparece no serial ao clicar. No hub, `[tcp] Command queued for ...` (modo TCP) ou `[ESP-NOW] cmd to <mac> hop 1/6` (modo ESP-NOW).
6. **Follow-up em aberto**: `dest_for_chip()` (hub) envia unicast ESP32→ESP8266 para o MAC WiFi do registry, enquanto a regra 19 pede derivar o MAC alt (`mywifi_espnow_mac`, hoje sem uso). Não alterado por falta de validação em bancada.

### Trabalho concluído nesta sessão (sessão 7 — comandos Telegram param de responder)

1. **Cursor do Telegram inicializado** (`hub/src/telegram_bot.cpp`): `last_message_received` começava sem valor definido na biblioteca; agora parte de 0.
2. **Conexão TLS encerrada após `getUpdates()`**: força cada resposta/consulta a iniciar conexão limpa, evitando depender de socket persistente potencialmente obsoleto.
3. **Alertas críticos sem repetição contínua**: bateria crítica e fumaça enviam imediatamente ao entrar em condição crítica e só podem enviar de novo após normalizar e reativar.
4. **Relatório diário consolidado** em uma única mensagem, evitando envio serial de uma mensagem por node.
5. **Validação**: `$HOME/.platformio/penv/bin/pio run -e hub_32_ota` passou (RAM 72.004 bytes; flash 1.446.210 bytes). Issue search não encontrou issue existente para esse sintoma.

### Trabalho concluído nesta sessão (sessão 6 — build hub_32_ota)

1. **Callback ESP-NOW do hub** (`hub/src/espnow_handler.cpp`): usa `esp_now_recv_info_t` e `src_addr` em ESP-IDF 5+, mantendo a assinatura antiga para versões anteriores e ESP8266.
2. **Callbacks no shared** (`shared/src/espnow_node_protocol.cpp`): adapta recepção para ESP-IDF 5+ e envio para ESP-IDF 5.5+, preservando APIs antigas e ESP8266.
3. **Validação**: `$HOME/.platformio/penv/bin/pio run -e hub_32_ota` passou. `firmware.bin` tem 1.482.448 bytes e cabe nos slots de 0x1D0000 configurados em `hub/partitions.csv`; não cabe em hubs com partição OTA antiga de 1 MiB (regra 35).

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
