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
- **Código compartilhado**: `shared/` na raiz é a fonte única de `espnow_protocol.h`, `myWiFiManager.h/.cpp` e `shared_config.h` (wrapper cross-platform WiFi ESP8266/ESP32). Gateway e clients DEVEM incluir `shared/` e NÃO manter cópias divergentes desses arquivos (regra 17 estendida: qualquer mudança de struct/protocolo ou WiFiManager vale para todos os devices e deve ser feita uma única vez em `shared/`).

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
18. **ESP-NOW broadcast vs unicast (MESMO AP)**: Dois modos existem — unicast (MAC específico) e broadcast (`FF:FF:FF:FF:FF:FF`). **ESP8266↔ESP8266 (homogêneo): unicast funciona bem nos dois sentidos** (o repeater envia unicast para seus clients peers) e é preferível para ACKs/comandos direcionados; broadcast também funciona. **Misto ESP32↔ESP8266**: validado com QuickESPNow (qgw ESP32 + qclient ESP8266, mesmo AP): **ESP8266→ESP32 unicast FALHA** (drop silencioso por coexistência rádio); **ESP32→ESP8266 unicast FUNCIONA**; **broadcast funciona nos dois sentidos**. Conclusão: **quem envia é ESP8266 e quem recebe é ESP32 → BROADCAST obrigatório** (ex: client ESP8266 → gateway ESP32: dados, heartbeat, PAIR_REQUEST); **ESP8266→ESP8266 ou ESP32→ESP8266 → unicast OK**. O fallback "unicast→broadcast se `esp_now_send` der erro" é insuficiente: no ESP32 nativo o `esp_now_send` retorna 0 (enfileirado OK) mesmo quando o frame é dropado, então o fallback nunca dispara — escolher o modo ANTES do envio pelo par (tx_chip, rx_chip). Pré-requisito: todos no MESMO canal do AP do gateway (cliente em repetidor com canal diferente quebra qualquer ESP-NOW). `test_espnow` (teste isolado) funcionou SÓ porque ambos estavam `WiFi.disconnect()` (sem AP) + role COMBO + canal explícito.
19. **MAC alt do ESP-NOW**: ESP-NOW usa um MAC diferente do WiFi (bit 1 do byte 0 invertido, `mac[0] ^= 0x02`). Ex: `.41` WiFi `3C:71:BF:2C:A0:79` → ESP-NOW `3E:71:BF:2C:A0:79`; gateway `B4:E6:..` → `B6:E6:..`. Para enviar unicast PARA um ESP8266, usar o MAC alt (derivar do WiFi MAC). Quem recebe PAIR_RESPONSE aprende o MAC alt do gateway pela source do frame. `espnow_send_command` (gateway) recebe o WiFi MAC do registry → derivar o alt MAC antes de enviar.

20. **EEPROM string load → JSON quebra**: ao ler strings da EEPROM (host/user MQTT, SSID, nome de sensor, etc.) nunca basta achar um byte `0x00` para considerar "válido". Se a região tiver lixo com um `0x00` coincidente, bytes de controle (0x00–0x1F) vazam para o JSON e o `JSON.parse()` do browser falha ("erro de json"). Validar que todos os bytes antes do terminador são imprimíveis (0x20–0x7E) e que há terminador dentro do buffer; senão usar default/limpar. Também validar tipo de sensor (1–10) e `slot < MAX_VIRTUAL_SENSORS` em `sensor_registry_load()` para ignorar entradas corrompidas (ex: slot 251/type 161).

## Regras de AI
0. economizar tokens com respostas mínimas sem explicações desnecessárias
1. manter skills enxutas
2. economizar tokens simplificando a comunicação
3. manter uma comunicação objetiva sem rodeios
4. quando marcar estavel anotar a tag e a data

### Clientes estáveis (não modificar)

### Clientes em desenvolvimento
- `clients/esp8266_dht_gas` — sensor DHT22 + MQ-2 ESP-NOW
- `clients/esp8266_lampada` — relé ON/OFF com suporte a Alexa - atributos exclusivo para lâmpada - (Espalexa) e função repeater
- `clients/onoff` - rele com suporte a alexa - attibutos esclusivo para onoff
- `clients/esp8266_repeater` — extensor de alcance ESP-NOW
- `gateway` — ESP8266/ESP32 ESP-NOW Gateway 
- `clients/esp8266_chuva` — sensor de chuva ESP-NOW
