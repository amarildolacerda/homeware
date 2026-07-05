# HA Bridge API + Gateway ESPNOW - ESPNOW Clients — Projeto

## SubModules
- `bridge` - projeto bridge para rodar em esp32 com integração com RainMaker
- `clients` - clientes para o gateway
- `gateway` - projeto gateway para ESP usando protocolo ESPNOW e comunicação com o bridge

## Branches
- `main` — estável, usado nos dispositivos em produção
- `dev` — desenvolvimento
- a atualizao do "dev" para "main" so pode ser feito se solicitado ou pegar autorização
- `main-v0.0.3` — backup do main anterior (antes do reset para dev)
- antes de passar o dev para main gerar um branch do main_vx.x.x
- quando gerar uma nova tag (ex: v0.0.7) tornar a versão a mesma da tag em todos os bridges (C + Python) e clients ESP8266 (FW_VERSION)
- procurar por .git_token a ser usado com git

## Ambiente

### Pré-requisitos
- `platformio`
- Python 3.10+ com `venv`


## Scripts
- `build.sh` — só build
- `flash.sh [-p <port>]` — source + build + flash (porta padrão `/dev/ttyUSB0`)
- `monitor.sh` — source + monitor (saída: `Ctrl+]`)
- `monitor.py` — monitor serial Python, sai com `q` ou `Ctrl+C`
- `erase.sh` — source + erase-flash

## Arquitetura
- Bridge (HA python): servidor HTTP REST + HA + discovery UDP
- Clients (ESP8266/Arduino): sensores/atuadores que se registram no gateway via ESPNOW
- Discovery UDP: broadcast porta 5000, service name `"esp-bridge"`

## Terminal do Bridge (console serial)
- `l` — lista devices registrados (com índices numéricos)
- `s` — status do bridge (IP, total devices, uptime)
- `d <id|índice>` — detalhes de um device (aceita ID ou número da lista `l`)
- `b` — broadcast re-register (envia `re_register:true` via UDP, clients re-registram via HTTP, mostra registrados + descobertos)
- `r` — restart
- `h` / `?` — ajuda
- Usa `getchar()` single-key, prompt `bridge>` só aparece após comando executado
- para descobrir o ip do Bridge rodar discover_bridge.py

## Desenvolvimento
- Alterações de código devem ser feitas apenas no branch `dev`. Verifique com `git branch --show-current` antes de começar.
- `main` é estável e usado em produção — nunca commitar diretamente em `main`, os commits devem ser feito no dev
- Quando o dev passa para produção (main), fazer uma tag ,  fazer um merge do dev para o main, ficando o main no mesmo ponto do dev,  criar um branch <main_xxx>

- os dashboards devem ter uma url /docs para listar a api <swagger like>, se form um device manter enxuto para nao comprometer memoria - atualizar quando alteracoes na url forem implementadas/alteradas
- o dashboard precisa ser responsivo e tratar o loop para nao congelar a carga do browser

### Novos Clients
1. sempre ter um README.md para orientar as conexões de hardwares/pinos e demais informações releventes ao cliente
2. ter um dashboard pertinent 
3. se possivel ter configuração WIFI com WiFiManager
4. ter atalhos de teclados no terminal
5. seguir API do gateway
6. ter um SPECx.md para o especificação de funcionamento esperado

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
11. Bridge broadcast (`b`): envia `re_register:true` via UDP, gateway re-registram via HTTP, mostra registrados + descobertos
12. clients se cominica via ESPNOW com o gateway
12. Versão (tag) vale para gateway ESP, HA bridge Python e clients ESP8266 — todos devem ter a mesma FW_VERSION

## Regras de AI
0. economizar tokens com respostas minimas sem explicações desnecessaria 
1. manter skills enxutas
2. economizar tokens simplificando com a comunição
3. manter uma comunicação objetiva sem rodeios

### Clientes estáveis (não modificar)
- `gateway` — ESP8266 ESP-NOW Gateway (firmware estável em produção)
- `clients/esp8266_chuva` — sensor de chuva ESPNOW
