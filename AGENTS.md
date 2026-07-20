# HA Bridge API + Gateway ESPNOW - ESPNOW Clients — Projeto

## SubModules
- `bridge` - bridge Python para Home Assistant (Add-on / standalone)
- `clients` - clientes ESP8266 para o gateway ESP-NOW
- `gateway` - gateway ESP-NOW que recebe dados dos clients e encaminha ao bridge via HTTP

## Branches
- `main` — estável, usado nos dispositivos em produção
- `dev` — desenvolvimento
- atualização do "dev" para "main" só pode ser feito se solicitado ou pegar autorização
- antes de passar o dev para main gerar um branch do main_vx.x.x
- quando gerar uma nova tag (ex: v0.0.17) tornar a versão a mesma da tag em todos os bridges (Python) e clients ESP8266 (FW_VERSION)
- procurar por .git_token a ser usado com git
- **NÃO fazer push para `dev` automaticamente**: commitar localmente no dev durante o trabalho, mas só dar `push` para `origin/dev` quando o usuário confirmar que a implementação está concluída/refinada. Aguardar o sinal do usuário antes de subir.

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
- `scan.py` mostra os IPs com dispositivos e gateway conectados na LAN

## Arquitetura
- Bridge (HA Python): servidor HTTP REST + HA + discovery UDP + MQTT Discovery
- Gateway (ESP8266): recebe dados dos clients via ESP-NOW (canal 1), encaminha ao bridge via HTTP REST
- Clients (ESP8266/Arduino): sensores/atuadores que se registram no gateway via ESP-NOW
- Discovery UDP: broadcast porta 5000, service name `"esp-bridge"`
- // D1-MINI é invtido
#define LED_ON  LOW   // GPIO2 acende com LOW
#define LED_OFF HIGH  // GPIO2 apaga com HIGH

## Terminal do Bridge (console serial)
- `l` — lista devices registrados (com índices numéricos)
- `s` — status do bridge (IP, total devices, uptime)
- `d <id|índice>` — detalhes de um device (aceita ID ou número da lista `l`)
- `b` — broadcast re-register (envia `re_register:true` via UDP, clients re-registram via HTTP, mostra registrados + descobertos)
- `r` — restart
- `h` / `?` — ajuda
- Usa `getchar()` single-key, prompt `bridge>` só aparece após comando executado
- para descobrir o ip do Bridge: `scan.py` no root do projeto

## Desenvolvimento
- Alterações de código devem ser feitas apenas no branch `dev`. Verifique com `git branch --show-current` antes de começar.
- `main` é estável e usado em produção — nunca commitar diretamente em `main`, os commits devem ser feito no dev
- Quando o dev passa para produção (main), fazer uma tag, fazer um merge do dev para o main, ficando o main no mesmo ponto do dev, criar um branch <main_xxx>

- os dashboards devem ter uma url /docs para listar a api <swagger like>, se for um device manter enxuto para nao comprometer memoria - atualizar quando alteracoes na url forem implementadas/alteradas
- o dashboard precisa ser responsivo e tratar o loop para nao congelar a carga do browser
- dashboards de clients ESP8266 devem usar layout compacto com seção "Detalhes" collapsível (expandir ao clicar), mostrando apenas o controle principal + badge estado por padrão
- os arquivos fontes devem tentar organização separados por responsabilidade, tentar otimizar uso de memoria / PROGMEM nos ESP, deixando o main mais limpo, usar config.h para mapear configurações
- mostrar no dashboard a versão (FW_VERSION)
- quando conseguir resolver um problema, erro ou mudança de especificação - registrar a nova regra para aprendizado e reaproveitamento nas proximas sessões
- cuidado com chamadas repetitivas a api, reaproveitar quando for possivel
- **Código compartilhado**: `shared/` é um **submodule** (`homeware_shared.git`) e é a fonte única de código cross-platform (regra 17 estendida). Estrutura de lib PlatformIO: `library.json` na raiz + **todo o código (`.h` e `.cpp`) em `src/`**. Contém: `espnow_protocol.h`, `myWiFiManager.h/.cpp`, `shared_config.h`, `platform.h` (wrappers `MyWebServer`/`chip_id()`/`espnow_add_peer_wrapper`), `common_console.*` (telnet), `common_ota.*` (ArduinoOTA), `common_util.*` (`uptime_to_str`), `common_wifi.*` (delegators p/ myWiFiManager), `timer.*` (agendamento parametrizado). Gateway e clients DEVEM consumir via `lib_extra_dirs` (gateway: `../shared`; clients: `../../shared`) e NÃO manter cópias divergentes nem usar `-I` manual ou scripts de cópia. Qualquer mudança de struct/protocolo/WiFiManager/common vale para todos os devices e deve ser feita uma única vez no submodule `shared/` (commitar e pushar o submodule, depois bump no homeware). **Ao commitar/pushar o submodule `shared/`, usar SEMPRE o branch `dev` (nunca `main`) — o shared também segue a política de dev→main do homeware.**

### Novos Clients
1. sempre ter um README.md para orientar as conexões de hardwares/pinos e demais informações relevantes ao cliente
2. ter um dashboard pertinente
3. Se usa WiFi, usar myWiFiManager
4. ter atalhos de teclado no terminal
5. seguir API do gateway
6. ter um SPEC.md para especificação de funcionamento esperado
7. **perguntar ao usuário** se o cliente deve incluir funções de **repeater** ESP-NOW e quais regras usar (ex: repetir broadcast do gateway, encaminhar dados de outros clients). Se for repeater, deve ser **bidirecional** (gateway ↔ clientes em ambas as direções).
8. Incluir OTA

## Regras importantes
1. Device ID é dinâmico (`esp8266_<chip_id>`), não configurável
2. Device name configurável via WiFiManager, salvo em EEPROM com validação (> 32, < 127)
3. BRIDGE_HOST = "0.0.0.0" força discovery UDP (sem fallback fixo)
4. Sempre copiar cJSON `valuestring` para buffer local com `strncpy` antes de `cJSON_Delete`
5. Retry de registro no `loop()`, não só no `setup()`
6. `CONFIG_LWIP_MAX_SOCKETS` precisa ser aumentado se aparecer `ENFILE`
7. Persistir devices bridgeados em NVS para restaurar no boot
8. Clients enviam `bridge_connected` no `/api/state`
9. DHT21 client: GPIO 5, tipo DHT21, fallback `isnan()` não envia ao bridge (flag `s_dht_valid`)
10. Clients respondem a `re_register:true` no UDP registrando novamente via HTTP POST `/api/device/register`
11. Bridge broadcast (`b`): envia `re_register:true` via UDP, gateway re-registra via HTTP, mostra registrados + descobertos
12. Clients se comunicam via ESP-NOW com o gateway (NÃO HTTP direto)
13. Versão (tag) vale para gateway ESP, HA bridge Python e clients ESP8266 — todos devem ter a mesma FW_VERSION
14. Qualquer mudança de estado no client deve disparar feedback imediato ao gateway (setar `s_last_espnow_send = 0`)
15. **Loop non-blocking**: `loop()` não pode conter `delay()` bloqueante. Usar máquina de estados com timestamps (`millis()`) para ESP-NOW sends, ACK wait, retries, pareamento, heartbeat e LED. Aplica-se a gateway e todos os clients.
16. **Páginas web PROGMEM**: páginas HTML grandes (>10KB) via `FPSTR` + `send()` estouram heap no ESP8266 porque alocam String RAM. `send_P()` e `sendContent_P()` também falham se o buffer TCP encher (`write()` retorna 0). Para páginas grandes, escrever response manualmente via `WiFiClient` em chunks pequenos (256 bytes) com `yield()` entre chunks. Alternativa: manter páginas enxutas (<8KB) para usar `send_P()` sem risco.
17. **device_name[32]**: `espnow_pair_request_t.device_name` usa32 bytes (compatível com `s_device_name[32]` dos clients). `virtual_sensor_t.name` e `pending_pair_t.name` também32. EEPROM_SENSOR_SIZE=48 (nome ocupa32 bytes no offset9). Qualquer mudança nesse campo exige atualização simultânea de gateway e todos os clients.
18. **ESP-NOW broadcast vs unicast (MESMO AP)**: Dois modos existem — unicast (MAC específico) e broadcast (`FF:FF:FF:FF:FF:FF`). **ESP8266↔ESP8266 (homogêneo): unicast funciona bem nos dois sentidos** (o repeater envia unicast para seus clients peers) e é preferível para ACKs/comandos direcionados; broadcast também funciona. **Misto ESP32↔ESP8266**: validado com QuickESPNow (qgw ESP32 + qclient ESP8266, mesmo AP): **ESP8266→ESP32 unicast FALHA** (drop silencioso por coexistência rádio); **ESP32→ESP8266 unicast FUNCIONA**; **broadcast funciona nos dois sentidos**. Conclusão: **quem envia é ESP8266 e quem recebe é ESP32 → BROADCAST obrigatório** (ex: client ESP8266 → gateway ESP32: dados, heartbeat, PAIR_REQUEST); **ESP8266→ESP8266 ou ESP32→ESP8266 → unicast OK**. O fallback "unicast→broadcast se `esp_now_send` der erro" é insuficiente: no ESP32 nativo o `esp_now_send` retorna 0 (enfileirado OK) mesmo quando o frame é dropado, então o fallback nunca dispara — escolher o modo ANTES do envio pelo par (tx_chip, rx_chip). Pré-requisito: todos no MESMO canal do AP do gateway (cliente em repetidor com canal diferente quebra qualquer ESP-NOW). `test_espnow` (teste isolado) funcionou SÓ porque ambos estavam `WiFi.disconnect()` (sem AP) + role COMBO + canal explícito. **Bancada confirmada (2026-07-19, teste `tests/espnow_unicast_test/` gw32 ESP32 COM4 + client ESP8266 COM5, AP `kcasa` ch=4): client ESP8266 enviou alternando unicast/broadcast; BROADCAST `status=0` (OK), UNICAST `status=1` (FALHA no send-callback) — prova empírica da quebra do unicast ESP8266→ESP32 COM AP. Sem AP (STA disconnect) o mesmo unicast dava `status=0` e o gw32 recebia (`unicast=3`). Conclusão reforçada: em produção (gateway no AP) client ESP8266→gateway ESP32 DEVE usar broadcast; unicast só ESP32→ESP8266 ou ESP8266→ESP8266.
19. **MAC alt do ESP-NOW**: ESP-NOW usa um MAC diferente do WiFi (bit 1 do byte 0 invertido, `mac[0] ^= 0x02`). Ex: `.41` WiFi `3C:71:BF:2C:A0:79` → ESP-NOW `3E:71:BF:2C:A0:79`; gateway `B4:E6:..` → `B6:E6:..`. Para enviar unicast PARA um ESP8266, usar o MAC alt (derivar do WiFi MAC). Quem recebe PAIR_RESPONSE aprende o MAC alt do gateway pela source do frame. `espnow_send_command` (gateway) recebe o WiFi MAC do registry → derivar o alt MAC antes de enviar.

20. **EEPROM string load → JSON quebra**: ao ler strings da EEPROM (host/user MQTT, SSID, nome de sensor, etc.) nunca basta achar um byte `0x00` para considerar "válido". Se a região tiver lixo com um `0x00` coincidente, bytes de controle (0x00–0x1F) vazam para o JSON e o `JSON.parse()` do browser falha ("erro de json"). Validar que todos os bytes antes do terminador são imprimíveis (0x20–0x7E) e que há terminador dentro do buffer; senão usar default/limpar. Também validar tipo de sensor (1–10) e `slot < MAX_VIRTUAL_SENSORS` em `sensor_registry_load()` para ignorar entradas corrompidas (ex: slot 251/type 161).
21. **ESP8266 client DEVE usar `WiFi.setSleepMode(WIFI_NONE_SLEEP)` em `espnow_init_client`**: o modem-sleep padrão do ESP8266 desliga o rádio periodicamente e o client PERDE pacotes ESP-NOW recebidos (ex: PAIR_RESPONSE do gateway chega mas não é entregue à recv_cb → client fica enviando PAIR_REQUEST em loop, gateway re-emparelha, mas `s_paired` nunca atualiza). Sintoma clássico: client com RSSI bom, gateway recebe PAIR_REQUEST/SENSOR_DATA, mas client "não recebe" a resposta. Sempre espelhar a lampada (`espnow_init_client` com `WiFi.setSleepMode(WIFI_NONE_SLEEP)` antes do `esp_now_init`).

22. **MQTT Discovery topic sem ponto**: o tópico de discovery do HA (`homeassistant/<component>/<entity_id>/config`) NÃO permite o caractere `.` no `<entity_id>` (warning "illegal discovery topic"). O `entity_id` (montado em `mqtt_client.cpp:build_entity_id`) e o `bridge_device_id` devem usar `_` em vez de `.` como separador (ex: `gw_294F55_lgt_0`). O `.` no `device.identifiers` do payload JSON é aceito (não é tópico), mas manter `_` por consistência. **O slot é o ÚLTIMO segmento do entity_id após `_`** — o `mqtt_callback` (gateway) extrai o slot com `entity_id.lastIndexOf('_')`. NUNCA usar `.` para parsing do slot (quebra o comando HA→gateway→client). Sempre que alterar o formato do `entity_id`, atualizar o parser do slot no callback na mesma mudança.
23. **Gateway ESP32 pode travar intermitentemente (offline sem reboot)**: observado em 2026-07-19 (v0.0.27) — gateway parou de responder ping/HTTP/telnet e só voltou com power-cycle. Como o loopTask WDT (5s) reiniciaria se o `loop()` travase, o congelamento veio de task/ISR do ESP-NOW (recv_cb/send_cb rodam em task própria) ou exceção não recuperada. **Root cause still OPEN**: exigiu backtrace serial do gateway no momento do crash (não capturado). Para debugar, conectar o gateway via USB e capturar o exception/backtrace; não chutar fix. Suspeita: `console.printf` pesado dentro do recv_cb com telnet conectado + muito tráfego, ou concorrência em `sensor_registry` acessado do recv_cb e do loop. Cliente DHT_GAS só pareou depois do gateway voltar + sleep mode fix (regra 21).

## Regras de AI
0. economizar tokens com respostas mínimas sem explicações desnecessárias
1. manter skills enxutas
2. economizar tokens simplificando a comunicação
3. manter uma comunicação objetiva sem rodeios
4. quando marcar estavel anotar a tag e a data

### Clientes estáveis (não modificar)
- `clients/esp8266_lampada` — relé ON/OFF com suporte a Alexa (Espalexa) + função repeater ESP-NOW — estável em **v0.0.29** (2026-07-19)
- `gateway` — ESP8266/ESP32 ESP-NOW Gateway — estável em **v0.0.29** (2026-07-19)
- `clients/esp8266_dht_gas` — sensor DHT22 + MQ-2 ESP-NOW (dashboard Detalhes/OTA/toggles temp+gás, WIFI_NONE_SLEEP) — estável em **v0.0.29** (2026-07-19)

### Clientes em desenvolvimento
- `clients/onoff` - rele com suporte a alexa - attibutos esclusivo para onoff
- `clients/esp8266_repeater` — extensor de alcance ESP-NOW
- `clients/esp8266_chuva` — sensor de chuva ESP-NOW
