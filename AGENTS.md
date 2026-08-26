# AgriSense IoT — Projeto

## SubModules
- `server` - servidor Python para Home Assistant (Add-on / standalone)
- `nodes` - nós sensores/atuadores para o hub ESP-NOW
- `hub` - hub ESP-NOW que recebe dados dos nodes e encaminha ao server via HTTP

## Branches
- `main` — estável, usado nos dispositivos em produção; nunca commitar diretamente nele
- `dev` — desenvolvimento
- A atualização do `dev` para `main` só pode ser feita se solicitada ou com autorização.
- Antes de passar o `dev` para `main`, gerar um branch `main_vx.x.x`.
- Quando gerar uma tag (ex: v0.0.17), tornar a versão a mesma da tag em todos os servers (Python) e nodes (FW_VERSION).
- Procurar por `.env` (fallback `.git_token`) para usar com git/MQTT — credenciais ficam fora do repo versionado (`.env` ignorado no `.gitignore`; remotes SEM token embutido na URL).
- **NÃO fazer push para `dev` automaticamente**: commitar localmente no `dev` durante o trabalho, mas só dar `push` para `origin/dev` quando o usuário confirmar que a implementação está concluída/refinada. Aguardar o sinal do usuário antes de subir.
- pegar novas issue no github; se um bug foi encontrado e não tiver um issue para ele abrir uma nova; quando fechar a issue marcar fechado, explicar de forma simples o que foi feito;

## Ambiente

### Pré-requisitos
- `platformio`
- Python 3.10+ com `venv`

## Scripts
- `build.sh` — só build
- `flash.sh [-p <port>]` — source + build + flash (porta padrão `/dev/ttyUSB0`)
- `flash.ps1 [-p <port>] [-o <ip>]` — flash via serial ou OTA no Windows
- `monitor.sh` — source + monitor (saída: `Ctrl+]`)
- `monitor.ps1 [-p <port>]` — monitor serial no Windows
- `monitor.py` — monitor serial Python, sai com `q` ou `Ctrl+C`
- `erase.sh` — source + erase-flash
- `erase.ps1 [-p <port>]` — erase flash no Windows
- `scan.py` mostra os IPs com dispositivos e hub conectados na LAN

## Arquitetura
- Server (Python): servidor HTTP REST + HA + discovery UDP + MQTT Discovery
- Hub (ESP8266/ESP32): recebe dados dos nodes via ESP-NOW, encaminha ao server via HTTP REST
- Nodes (ESP8266): sensores/atuadores que se registram no hub via ESP-NOW
- **Diferenças entre plataformas** (ESP8266 vs ESP32) são tratadas em `shared/src/platform.h` (wrappers `MyWebServer`/`chip_id()`/`espnow_add_peer_wrapper`). Quando algo depende da plataforma, verificar/alterar `platform.h` — não duplicar `#ifdef` espalhado pelo código.
- Discovery UDP: broadcast porta 5000, service name `"esp-bridge"`
- // D1-MINI é invertido
#define LED_ON  LOW   // GPIO2 acende com LOW
#define LED_OFF HIGH  // GPIO2 apaga com HIGH

## Terminal do Server (console serial)
- `l` — lista devices registrados (com índices numéricos)
- `s` — status do server (IP, total devices, uptime)
- `d <id|índice>` — detalhes de um device (aceita ID ou número da lista `l`)
- `b` — broadcast re-register (envia `re_register:true` via UDP, nodes re-registram via HTTP, mostra registrados + descobertos)
- `r` — restart
- `h` / `?` — ajuda
- Usa `getchar()` single-key, prompt `server>` só aparece após comando executado
- para descobrir o ip do Server: `scan.py` no root do projeto

## Desenvolvimento
- Alterações de código devem ser feitas apenas no branch `dev`. Verifique com `git branch --show-current` antes de começar.
- A política de branches (dev→main, tags, push) está descrita na seção **Branches** acima.

- os dashboards devem ter uma URL `/docs` para listar a API (estilo Swagger); se for um device, manter enxuto para não comprometer a memória — atualizar quando a URL for alterada
- o dashboard precisa ser responsivo e tratar o loop para não congelar a carga do browser
- dashboards de nodes devem usar layout compacto com seção "Detalhes" colapsável (expandir ao clicar), mostrando apenas o controle principal + badge de estado por padrão
- os arquivos-fonte devem ser organizados por responsabilidade, otimizando o uso de memória / PROGMEM nos ESP, deixando o `main` mais limpo e usando `config.h` para mapear configurações
- mostrar no dashboard a versão (FW_VERSION)
- checar tests unitários para novas implementações para garantir que as regras não estejam quebradas; se uma regra não tiver test unitário, criar um para garantir o futuro. Se uma implementação altera um test que passou a ter novas características/regras, mudar o test. Se uma implementação quebra uma regra, incluir isso como parte da decisão de implementação;
- para decidir se uma implementação é de uso shared ou individual, verificar se há reaproveitamento em várias implementações (nodes); se for reaproveitada, implementar no shared; caso contrário, é individual (não compartilhada);
- adotar paradigma clean e fugir de acoplamentos;
- separar types que são compartilhados para evitar acoplamentos, locais que são especialidades locais;
- separar defines compartilhados e locais o que são somente de uso local;
- evitar variáveis globais, preferir classes;
- para implementações (strategy) preferir interfaces a serem herdadas (ex: RadioInterface);
- quando conseguir resolver um problema, erro ou mudança de especificação, registrar a nova regra para aprendizado e reaproveitamento nas próximas sessões
- cuidado com chamadas repetitivas à API, reaproveitar quando for possível
- **Código compartilhado**: `shared/` é um **submodule** (`homeware_shared.git`) e é a fonte única de código cross-platform (regra 17 estendida). Estrutura de lib PlatformIO: `library.json` na raiz + **todo o código (`.h` e `.cpp`) em `src/`**. Contém: `espnow_protocol.h`, `myWiFiManager.h/.cpp`, `shared_config.h`, `platform.h` (wrappers `MyWebServer`/`chip_id()`/`espnow_add_peer_wrapper`), `common_console.*` (telnet), `common_ota.*` (ArduinoOTA), `common_util.*` (`uptime_to_str`), `common_wifi.*` (delegators p/ myWiFiManager), `timer.*` (agendamento parametrizado). Hub e nodes DEVEM consumir via `lib_extra_dirs` (hub: `../shared`; nodes: `../../shared`) e NÃO manter cópias divergentes nem usar `-I` manual ou scripts de cópia. Qualquer mudança de struct/protocolo/WiFiManager/common vale para todos os devices e deve ser feita uma única vez no submodule `shared/` (commitar e pushar o submodule, depois bump no homeware). **Ao commitar/pushar o submodule `shared/`, usar SEMPRE o branch `dev` (nunca `main`) — o shared também segue a política de dev→main do homeware.**

### Novos Nodes
Ver `nodes/SPEC.md` — checklist completo com template, estrutura, implementação e regras.

## Regras importantes
1. Device ID é dinâmico (`agri_<chip_id>`), não configurável
2. Device name configurável via WiFiManager, salvo em EEPROM com validação (> 32, < 127)
3. SERVER_HOST = "0.0.0.0" força discovery UDP (sem fallback fixo)
4. Sempre copiar cJSON `valuestring` para buffer local com `strncpy` antes de `cJSON_Delete`
5. Retry de registro no `loop()`, não só no `setup()`
6. `CONFIG_LWIP_MAX_SOCKETS` precisa ser aumentado se aparecer `ENFILE`
7. Persistir devices bridgeados em NVS para restaurar no boot
8. Nodes enviam `bridge_connected` no `/api/state`
9. DHT21 node: GPIO 5, tipo DHT21, fallback `isnan()` não envia ao server (flag `s_dht_valid`)
10. Nodes respondem a `re_register:true` no UDP registrando novamente via HTTP POST `/api/device/register`
11. Server broadcast (`b`): envia `re_register:true` via UDP, hub re-registra via HTTP, mostra registrados + descobertos
12. Nodes se comunicam via ESP-NOW com o hub (NÃO HTTP direto)
13. Versão (tag) vale para hub, server Python e nodes — todos devem ter a mesma FW_VERSION
14. Qualquer mudança de estado no node deve disparar feedback imediato ao hub (setar `s_last_espnow_send = 0`)
15. **Loop non-blocking**: `loop()` não pode conter `delay()` bloqueante. Usar máquina de estados com timestamps (`millis()`) para ESP-NOW sends, ACK wait, retries, pareamento, heartbeat e LED. Aplica-se a hub e todos os nodes.
16. **Páginas web PROGMEM**: páginas HTML grandes (>10KB) via `FPSTR` + `send()` estouram heap no ESP8266 porque alocam String RAM. `send_P()` e `sendContent_P()` também falham se o buffer TCP encher (`write()` retorna 0). Para páginas grandes, escrever response manualmente via `WiFiClient` em chunks pequenos (256 bytes) com `yield()` entre chunks. Alternativa: manter páginas enxutas (<8KB) para usar `send_P()` sem risco.
17. **device_name[32]**: `espnow_pair_request_t.device_name` usa 32 bytes (compatível com `s_device_name[32]` dos nodes). `virtual_sensor_t.name` e `pending_pair_t.name` também 32. EEPROM_SENSOR_SIZE=58 (nome ocupa 32 bytes no offset 9, bridge_device_id 15 bytes no offset 43). Qualquer mudança nesse campo exige atualização simultânea de hub e todos os nodes.
18. **ESP-NOW broadcast vs unicast (MESMO AP)**: Dois modos existem — unicast (MAC específico) e broadcast (`FF:FF:FF:FF:FF:FF`). **ESP8266↔ESP8266 (homogêneo): unicast funciona bem nos dois sentidos** (o extender envia unicast para seus nodes peers) e é preferível para ACKs/comandos direcionados; broadcast também funciona. **Misto ESP32↔ESP8266**: validado com QuickESPNow (qgw ESP32 + qnode ESP8266, mesmo AP): **ESP8266→ESP32 unicast FALHA** (drop silencioso por coexistência rádio); **ESP32→ESP8266 unicast FUNCIONA**; **broadcast funciona nos dois sentidos**. Conclusão: **quem envia é ESP8266 e quem recebe é ESP32 → BROADCAST obrigatório** (ex: node ESP8266 → hub ESP32: dados, heartbeat, PAIR_REQUEST); **ESP8266→ESP8266 ou ESP32→ESP8266 → unicast OK**. O fallback "unicast→broadcast se `esp_now_send` der erro" é insuficiente: no ESP32 nativo o `esp_now_send` retorna 0 (enfileirado OK) mesmo quando o frame é dropado, então o fallback nunca dispara — escolher o modo ANTES do envio pelo par (tx_chip, rx_chip). Pré-requisito: todos no MESMO canal do AP do hub (node em extender com canal diferente quebra qualquer ESP-NOW). `test_espnow` (teste isolado) funcionou SÓ porque ambos estavam `WiFi.disconnect()` (sem AP) + role COMBO + canal explícito. **Bancada confirmada (2026-07-19, teste `tests/espnow_unicast_test/` gw32 ESP32 COM4 + node ESP8266 COM5, AP `kcasa` ch=4): node ESP8266 enviou alternando unicast/broadcast; BROADCAST `status=0` (OK), UNICAST `status=1` (FALHA no send-callback) — prova empírica da quebra do unicast ESP8266→ESP32 COM AP. Sem AP (STA disconnect) o mesmo unicast dava `status=0` e o gw32 recebia (`unicast=3`). Conclusão reforçada: em produção (hub no AP) node ESP8266→hub ESP32 DEVE usar broadcast; unicast só ESP32→ESP8266 ou ESP8266→ESP8266.
19. **MAC alt do ESP-NOW**: ESP-NOW usa um MAC diferente do WiFi (bit 1 do byte 0 invertido, `mac[0] ^= 0x02`). Ex: `.41` WiFi `3C:71:BF:2C:A0:79` → ESP-NOW `3E:71:BF:2C:A0:79`; hub `B4:E6:..` → `B6:E6:..`. Para enviar unicast PARA um ESP8266, usar o MAC alt (derivar do WiFi MAC). Quem recebe PAIR_RESPONSE aprende o MAC alt do hub pela source do frame. `espnow_send_command` (hub) recebe o WiFi MAC do registry → derivar o alt MAC antes de enviar.

20. **EEPROM string load → JSON quebra**: ao ler strings da EEPROM (host/user MQTT, SSID, nome de sensor, etc.) nunca basta achar um byte `0x00` para considerar "válido". Se a região tiver lixo com um `0x00` coincidente, bytes de controle (0x00–0x1F) vazam para o JSON e o `JSON.parse()` do browser falha ("erro de json"). Validar que todos os bytes antes do terminador são imprimíveis (0x20–0x7E) e que há terminador dentro do buffer; senão usar default/limpar. Também validar tipo de sensor (1–10) e `slot < MAX_VIRTUAL_SENSORS` em `sensor_registry_load()` para ignorar entradas corrompidas (ex: slot 251/type 161).
21. **Node DEVE usar `WiFi.setSleepMode(WIFI_NONE_SLEEP)` em `espnow_init_client`**: o modem-sleep padrão do ESP8266 desliga o rádio periodicamente e o node PERDE pacotes ESP-NOW recebidos (ex: PAIR_RESPONSE do hub chega mas não é entregue à recv_cb → node fica enviando PAIR_REQUEST em loop, hub re-emparelha, mas `s_paired` nunca atualiza). Sintoma clássico: node com RSSI bom, hub recebe PAIR_REQUEST/SENSOR_DATA, mas node "não recebe" a resposta. Sempre espelhar a lâmpada (`espnow_init_client` com `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes do `esp_now_init`).

22. **MQTT Discovery topic sem ponto**: o tópico de discovery do HA (`homeassistant/<component>/<entity_id>/config`) NÃO permite o caractere `.` no `<entity_id>` (warning "illegal discovery topic"). O `entity_id` (montado em `mqtt_client.cpp:build_entity_id`) e o `bridge_device_id` devem usar `_` em vez de `.` como separador (ex: `gw_294F55_lgt_0`). O `.` no `device.identifiers` do payload JSON é aceito (não é tópico), mas manter `_` por consistência. **O slot é o ÚLTIMO segmento do entity_id após `_`** — o `mqtt_callback` (hub) extrai o slot com `entity_id.lastIndexOf('_')`. NUNCA usar `.` para parsing do slot (quebra o comando HA→hub→node). Sempre que alterar o formato do `entity_id`, atualizar o parser do slot no callback na mesma mudança.
23. **Hub ESP32 pode travar intermitentemente (offline sem reboot)**: observado em 2026-07-19 (v0.0.27) — hub parou de responder ping/HTTP/telnet e só voltou com power-cycle. Como o loopTask WDT (5s) reiniciaria se o `loop()` travase, o congelamento veio de task/ISR do ESP-NOW (recv_cb/send_cb rodam em task própria) ou exceção não recuperada. **Root cause still OPEN**: exigiu backtrace serial do hub no momento do crash (não capturado). Para debugar, conectar o hub via USB e capturar o exception/backtrace; não chutar fix. Suspeita: `console.printf` pesado dentro do recv_cb com telnet conectado + muito tráfego, ou concorrência em `sensor_registry` acessado do recv_cb e do loop. Node DHT_GAS só pareou depois do hub voltar + sleep mode fix (regra 21).
24. **Nodes bateria (`-bat`)**: nodes com deep sleep usam sufixo `-bat` no nome da pasta (`soil-moisture`, `presence-bat`). Seguem o modelo: deep sleep entre ciclos, telnet suspende sleep (via `console.telnet_connected()`), telnet disconnect faz restart, comando `p` re-pareia, restart via `ESPNOW_MSG_RESTART` do hub, menu serial/telnet com `s/i/r/p/h`, intervalo configurável via WiFiManager.
25. **Router único por hub**: hub e todos os nodes DEVEM estar no MESMO roteador/AP. ESP-NOW usa o canal do roteador (`WiFi.channel()`). Se um node estiver em roteador diferente ou em modo AP isolado, o canal não bate e ESP-NOW não funciona. Não fixamos canal fixo nos clientes — eles herdam o canal do STA (WiFi). Se o roteador cair, hub e nodes permanecem no último canal conhecido (STA mantém o canal).
26. **WiFi não-bloqueante**: nunca usar `wm.autoConnect()` (bloqueante). Usar `myWiFiManager` do shared: `mywifi_begin(false)` tenta conectar com credenciais salvas; `mywifi_loop()` + `handle_wifi()` no `loop()` monitora conexão e abre AP customizado após timeout (120s). AP inicia com `WiFi.mode(WIFI_AP_STA)` + `DNSServer` para captive portal. Página de configuração: `PAGE_WIFI_CONFIG` com formulário SSID/password/device_name.
27. **`LORA_DEVICE` flag**: nós LoRa DEVEM definir `-DLORA_DEVICE` em `platformio.ini`. Isso evita incluir `<esp_now.h>` e compilar funções ESP-NOW. `lora_protocol.h` NÃO inclui `espnow_protocol.h` — tipos como `SENSOR_TYPE_ONOFF` são definidos localmente.
28. **Dashboard padrão**: seguir o layout dos nodes estáveis (lamp/climate-gas/presence): sidebar fixa 180px com nav (Home/Propriedades/Config), stats-header (RX/TX/Mem/Uptime), content `max-width:480px`, footer-bar com dot gateway + clock + uptime. JS: `setInterval(fetchState, 3000)`, `?from=` back link, `toggleRelay()` com loading guard, `doUpdate()` para OTA via XHR.
29. **API endpoints obrigatórios**: `/api/state` (estado + metadados), `/api/settings` GET/POST (device_name), `/api/wifi` GET/POST (credenciais WiFi), `/api/restart` POST, `/api/ota` POST (upload firmware via XHR + `Update.h`). Retornar `uptime_s`, `rx_count`, `tx_count`, `free_heap`, `fw_version`, `device_name` no `/api/state`.
30. **Console/telnet**: incluir `common_console.h`, chamar `console.begin()` + `console.set_banner()` no setup, `console.loop()` no loop, tratar `Serial.available()` e `console.telnet_read()` com `handle_serial()` com menu (h/s/l/p/r).
31. **EEPROM layout**: usar `EEPROM_SIZE` do shared (512). `EEPROM_RELAY_STATE` deve ficar em offset >=200 para não conflitar com shared (que usa 0-127 para WiFi/name).
32. **Board pinos**: respeitar defines da placa (`pins_arduino.h`). Ex: TTGO LoRa32 V1 `LORA_RST=14` (não 23). Verificar antes de definir.
33. **ESP32 WiFiManager**: usar `tzapu/WiFiManager @ ^2.0` (não `^0.16`). A v0.x só suporta ESP8266.
34. **updates**: uptime é estático e não muda no dashboard
35. **ESP-NOW command reliability ("fila com hops")**: o hub deve enfileirar comandos de relé (`MSG_COMMAND`)/`restart` (`MSG_RESTART`) em uma fila de retry de hop até `CMD_MAX_HOPS` (6) — `esp_now_send` no ESP32 aceita o quadro mesmo quando dropado em radio (coexistência, rule 18), então sem retry o comando é perdido para sempre. Reenvio a cada `CMD_HOP_INTERVAL_MS` (250ms), TTL `CMD_TTL_MS` (10s); comandos on/off são idempotentes (`set_relay(command==0x01)`, rule TCP 12) por isso hops repetidos não alternam o relé duas vezes. Implementado em `hub/include/espnow_handler.h` + `hub/src/espnow_handler.cpp` (`enqueue_cmd`/`process_pending_commands`/`loop()`). Sem o `send_cb`, o ESP32 não reporta drop → queue de hops é o mecanismo de confiabilidade.
36. **Lamp TCP registra como LIGHT**: o `lamp` env `esp8266_tcp` (HTTP + Alexa light entity) deve registrar `SENSOR_TYPE_LIGHT` (9), não `SENSOR_TYPE_ONOFF` (8) — `get_sensor_type()` retorna LIGHT com `-DTCP_ENABLED`, ONOFF caso contrário. Regra aprendida 2026-08-03.
37. **Alexa/Espalexa onNotFound**: o ESP8266WebServer perde o `onNotFound` quando `server->begin()` é chamado duas vezes (uma no setup, outra no `Espalexa::begin()`). Solução: re-registrar `onNotFound` imediatamente após `s_alexa.begin()` no WiFi connect. O handler deve chamar `s_alexa.handleAlexaApiCall(s_server.uri(), s_server.arg("plain"))` e retornar 404 se não for chamada Alexa. Sem isso, POST `/api` (devicetype) e GET `/api/<user>/lights` retornam 404 e o Alexa não consegue completar o discovery/pairing. Regra aprendida 2026-08-04.
38. **Alexa/Espalexa device type**: nodes relay (lamp, switch) com Alexa devem registrar `EspalexaDeviceType::onoff` (não `dimmable`). Usar `SENSOR_TYPE_ONOFF` no get_sensor_type(). Regra aprendida 2026-08-04.
39. **Padronização de flags -D**: usar padrão `FEATURE_ENABLED` em inglês. `HABILITA_REPEATER` → `REPEATER_ENABLED`, `HABILITA_PINOS` → `PINS_ENABLED`. Regra aprendida 2026-08-04.
40. **Timer (agendamento) funciona offline**: `timer_check` deve rodar mesmo sem WiFi (principal uso é offline — ligar/desligar relé em horários sem conectividade). Única exceção: quando o relógio ainda não foi carregado (`get_epoch() == 0` / `s_synced_epoch == 0`) — sem epoch não há como avaliar a hora corrente. Não condicionar `timer_check` a `WiFi.status() == WL_CONNECTED`. Regra aprendida 2026-08-05.
41. **Watchdog de blink: recomputar estado a cada loop, não usar latch `static`**: um `static bool in_wd_active = !(WiFi.status()==WL_CONNECTED)` inicializa UMA vez (no boot, WiFi normalmente ainda não conectou → inicia `true`) e, se só for setado para `true` (nunca volta a `false`), desabilita o watchdog (`watchdog_last_blink` fica 0 → condição `> 0` nunca satisfeita) ou o deixa disparar para sempre. Recomputar o estado ativo a cada `loop()` a partir do estado atual (WiFi perdido / portal / gateway sem parear). Regra aprendida 2026-08-05.
42. **TCP hub watchdog só renova em contato real**: `m_last_hub_contact_ms` deve ser atualizado apenas quando `send_to_hub()` retorna sucesso (HTTP 200). Se atualizado incondicionalmente após o envio, o watchdog de hub (`TCP_HUB_WATCHDOG_MS`) nunca dispara mesmo com o hub inacessível — o node fica piscando (gateway blink, `is_paired()`=`m_registered`) e nunca reinicia. Regra aprendida 2026-08-05.
43. **Watchdog estável (`StableWatchdog`)**: usar a classe `StableWatchdog` do shared (`common_watchdog.h`) para watchdogs à prova de flip-flop — o timer de restart só é (re)armado após `stable_reset_ms` contínuos e saudáveis; reconexões breves não desarmam. API: `init(stable_reset_ms, restart_ms, arm_from_start)`, `check(healthy)` no `loop()`, `reset()`. `arm_from_start=true` faz um device que nunca fica saudável reiniciar mesmo assim (auto-recuperação). Usado no lamp (blink watchdog) e no `TcpNodeProtocol` (WiFi watchdog). Regra aprendida 2026-08-05.
44. **`handleClient()` sempre no loop com Alexa**: em node relay com Espalexa (lamp), quando `s_alexa_initialized` é true o loop rodava só `s_alexa.loop()` num `else` e **nunca chamava `s_server.handleClient()`** → dashboard/API travados (porta 80 aceita TCP mas nunca responde HTTP) enquanto Alexa ativo. Sempre chamar `handleClient()` incondicionalmente após `s_alexa.loop()`. Sintoma clássico: dashboard morto no browser, telnet/serial ok. Regra aprendida 2026-08-11.
45. **Canal WiFi 255 (lixo EEPROM)**: `EEPROM_WIFI_CHANNEL_OFFSET` (=163, shared_config.h) nunca gravado = `0xFF` = 255. `sh_creds_load()` deve validar: `if (s_wifi_channel > 13) s_wifi_channel = 0;` (0 = auto, 1-13 = forcado). Sem isso, com `ESPNOW_ENABLED` o `s_wifi_channel > 0` com 255 dispara scan/mismatch (derruba STA, regra 44 antiga) e o dashboard `/api/wifi` mostra `channel: 255`. Fix no shared (myWiFiManager.cpp). Regra aprendida 2026-08-11.
46. **Guard de POST do TCP node deve checar IP (não só WL_CONNECTED)**: durante reconexão o `WiFi.status()` volta a `WL_CONNECTED` **antes** do DHCP atribuir o IP — o node tenta POST ao hub sem rota e falha com `httpCode=-1` (sintoma: `state failed (-1)` enquanto `heartbeat` aborta). Guard: `WiFi.status() != WL_CONNECTED || !wifi_has_ip()` (`(uint32_t)WiFi.localIP() != 0`; `IPAddress::isNone()` NÃO existe no ESP8266). Aplicado em `send_to_hub`, `publish_state` e `check_commands` do `tcp_node_protocol.cpp`. Regra aprendida 2026-08-11.
47. **Paridade de feedback MQTT entre rádios**: todo caminho que recebe estado do node deve terminar em `mqtt_client_publish_state()` — ESP-NOW via `queue_bridge_state`→`process_bridge_queue` (`espnow_handler.cpp`), LoRa via `lora_process_bridge_queue` (`main.cpp`), TCP direto no fim de `TcpRadioHandler::handle_state` (`tcp_radio_handler.cpp`). Sem o publish no caminho TCP, entidades HA não-otimistas (discovery sem `optimistic`) ficam sempre "off" mesmo com o relé mudando — comando chega ao node, mas o `state_topic` nunca é atualizado. Diagnóstico rápido: subscrever `homeassistant/<component>/+/state` no broker e publicar no `/set`; se nenhum `/state` chegar em ~10s, o feedback do rádio está quebrado. Regra aprendida 2026-08-25.
48. **Timer anti-re-fire deve ser baseado em epoch, não minute_id**: `s_last_fired_minute[i]` usando `hour*60+min` (0–1439) bloqueia re-fire no dia seguinte no mesmo horário (ex: 00:00 dispara dia 1, nunca mais). Usar janela de tempo (epoch) com threshold de 120s — só evita re-fire dentro do mesmo minuto. Declarar `s_last_fired_epoch[]` ao lado de `s_last_fired_minute[]`. Regra aprendida 2026-08-26.
49. **`cyclic_reset()` NÃO pode ser chamado quando o cyclic他自己 dispara OFF**: `set_relay(false)` chamava `cyclic_reset()` incondicionalmente, zerando `s_cyclic_next_event`; ~1s depois `cyclic_check()` via `now_ms >= 0` retornava ON. Fix: adicionar `from_cyclic` ao `set_relay()`, chamar `cyclic_reset()` só quando `!from_cyclic`. Aplica-se a lamp e switch. Regra aprendida 2026-08-26.
50. **Pulso (auto-OFF) precisa de lógica runtime**: ter UI + API + persistência não executa nada — precisa de `pulse_start()` (chamado em `set_relay(true)`), `pulse_cancel()` (em `set_relay(false)`), e `pulse_check(now_ms)` no `loop()`. Integrar no shared (`timer.h/.cpp`) para reaproveitamento entre nodes. Regra aprendida 2026-08-26.

### Nodes TCP (TCP_ENABLED / lamp TCP_RADIO + hub TcpRadioHandler)
- **ArduinoJson v7 `to<T>()` vs `as<T>()`**: `as<JsonObject>()` em documento null cria JsonObject que descarta silenciosamente assigns. Hub `TcpRadioHandler::handle_command_get` usava `response.as<JsonObject>()` — response sempre era `"null"` (4 bytes). Correção: `response.to<JsonObject>()`. Para verificar chave: `doc.containsKey("key") && !doc["key"].isNull()` em vez de `doc["key"].is<const char*>()`. Regra aprendida 2026-08-03.
- **TCP node publica estado periodicamente**: `TcpNodeProtocol::loop()` deve chamar `publish_state()` a cada `m_state_interval_ms` (ajuste de regra 14) e também logo após o registro bem-sucedido, senão o hub nunca aprende o IP do node. O intervalo `m_state_interval_ms` existia mas era ignorado.
- **Campo de estado on/off**: o node TCP envia `state` (bool) no POST `/node/state`; o hub (`TcpRadioHandler::handle_state`) deve aceitar tanto `state` quanto `relay_state` (bool ou uint8_t) e armazenar `onoff.state`. Não confiar em `is<uint8_t>()` para bools serializados como `true/false`. **O switch `handle_state` DEVE incluir `case SENSOR_TYPE_LIGHT:` junto com `case SENSOR_TYPE_ONOFF:`** — senão nodes LIGHT (tipo 9) nunca atualizam state no hub. Regra aprendida 2026-08-03.
- **IP do node**: o hub só aprende o IP do node TCP a partir do `ip` enviado em `/node/state` — fazer o `handle_state` fazer parse de `ip` (e `free_heap`) em `virtual_sensor_t`.
- **on_command semântico**: `on_command(command)` deve `set_relay(command == 0x01)`, não apenas togglear em 0x01 — senão comandos OFF (0x00) do hub são ignorados (botão desligar não age no node).
- **Todo evento de mudança de estado** (botão físico, timer, API `/api/relay`, Alexa, comando) deve disparar `s_radio.publish_state()` imediatamente (regra 14); o caminho do botão físico no lamp `loop()` não publicava.
- **Tamanho do firmware do hub ESP32 vs partição**: o hub físico em produção foi flashado via serial na era das partições de **1MB** (app0/app1 `0x100000`). OTA (ArduinoOTA) NÃO altera a tabela de partições — só grava o app. Então qualquer build do hub com flags `ESPNOW_ENABLED`/`TCP_ENABLED` que passe de `0x100000` (1.048.576 bytes) é **rejeitado pelo `Update.begin()`** (sintoma: espota responde `OK` no UDP 3232 mas nunca conecta TCP; `-f` errado ou tamanho > partição). Para caber: usar `-D CORE_DEBUG_LEVEL=0` (remove strings de log do framework) — com isso o build fica ~1.031.117 bytes. Se for preciso reflashar a tabela de partições de verdade, é necessário flash serial (esptool), não OTA. Atenção a caminho relativo: `.pio/build/hub_32_ota/firmware.bin` a partir da raiz aponta para build raiz desatualizado — usar caminho absoluto `hub\.pio\build\hub_32_ota\firmware.bin`.
- **Log "ESP-NOW initialized" e verificação de canal só para `ESPNOW_ENABLED`**: nodes TCP puros (env sem `-DESPNOW_ENABLED`) não inicializam ESP-NOW (`TcpNodeProtocol::begin()` é HTTP/UDP puro), mas (a) o log `console.printf("[%s] ESP-NOW initialized on ch %d\n", ...)` em `nodes/lamp/src/main.cpp:511` era impresso incondicionalmente e (b) o bloco de verificação/força de canal no `mywifi_loop()` (mismatch → `scanNetworks()` → `WiFi.disconnect()`+`begin()` com BSSID) rodava em todo node, mesmo TCP puro. Com `s_wifi_channel=255` (lixo da EEPROM, `>0` é true) o mismatch disparava o scan que **derrubava o STA** logo após conectar/registrar no hub → watchdog TCP reiniciava em loop. Fix: `#ifdef ESPNOW_ENABLED` no log e em todo o bloco de canal do `mywifi_loop()`. Sintoma clássico em TCP puro: conecta (IP), registra no hub, depois `WIFI Desconectado` + `[tcp-node] WiFi offline for too long, restarting...` em ciclo. Regra aprendida 2026-08-10.

## Regras de AI
0. economizar tokens com respostas mínimas sem explicações desnecessárias
1. manter skills enxutas
2. economizar tokens simplificando a comunicação
3. manter uma comunicação objetiva sem rodeios
4. quando marcar estável, anotar a tag e a data

### Nodes estáveis (não modificar)
- `hub` — v1.2.21 (2026-08-25) — ESP-NOW + TCP Hub, WiFi 3-tier (EEPROM→STATIC_WIFI→AP), NTP+time sync broadcast, MQTT discovery
- `nodes/lamp` — v1.2.8 (2026-08-05) — relé ON/OFF com Alexa (Espalexa) + extender ESP-NOW, usa shared WiFi
- `nodes/switch` — v1.2.8 (2026-08-05) — relé ON/OFF com Alexa (Espalexa) + ESP-NOW + pulse/timer, LoRa envs prontos

### Nodes em desenvolvimento
- `nodes/climate-gas` — sensor DHT22 + MQ-2 ESP-NOW (dashboard Detalhes/OTA/toggles temp+gás, WIFI_NONE_SLEEP)
- `nodes/presence` — sensor PIR ESP-NOW (broadcast, OTA, /api/restart)
- `nodes/onoff-lora` — relé LoRa (SX1278 + TTGO LoRa32 V1), dashboard padrão, WiFi não-bloqueante, OTA, console/telnet
- `nodes/extender` — extensor de alcance ESP-NOW
- `nodes/rain` — sensor de chuva ESP-NOW

## TODO / Futuro
- **Hub IP fixo no node TCP**: atualmente o `TcpNodeProtocol` descobre o hub via UDP broadcast (primeiro que responder ganha). Se 2 hubs estiverem na mesma rede, o node registra em um apenas. Solução: campo "Hub IP" no WiFiManager (salvo na EEPROM). Se configurado, o node pula o UDP discover e registra direto no hub configurado. Se não, usa o comportamento atual.

## Sanitize
- se algum texto contiver vícios de linguagem, corrigir e apresentar o texto corrigido em seguida; não dispensar acentuações, concordância ou vícios sintáticos