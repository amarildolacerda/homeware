# ESP8266 Clients Redesign — chuva, dht_gas, repeater

## Objetivo

Aplicar o mesmo redesign de dashboard feito para `onoff`/`lampada` nos três clients restantes: `chuva`, `dht_gas` e `repeater`. Cada cliente adapta o layout compartilhado (sidebar, stats-header, footer, docs) ao seu tipo de sensor, sem alterar a lógica de sensores, ESP-NOW, pareamento ou EEPROM.

## Escopo

- `clients/esp8266_chuva` — sensor de chuva (SENSOR_TYPE_RAIN)
- `clients/esp8266_dht_gas` — sensor DHT22 + MQ-2 (SENSOR_TYPE_DHT_GAS)
- `clients/esp8266_repeater` — extensor ESP-NOW (sem sensor)

## O que muda em cada cliente

### 1. Dashboard (pages.h)

Estrutura comum (inspirada no `onoff`):

```
Sidebar:
  🏠 Home        (sensor read-only — sem botão liga/desliga)
  📋 Propriedades (apenas chuva/dht_gas — repeater não tem)
  ⚙ Configurações

Stats-header (3–4 stats):
  RX · TX · Mem + 1 stat específico do cliente

Footer:
  ● Online/Offline · | · HH:MM · | · 0m

Seção "Detalhes" collapsível (apenas chuva/dht_gas) abaixo do hero.
```

#### chuva
- **Hero**: nível da chuva em % (grande, colorido) + badge textual (SECO/CHUVISCO/CHUVENDO/CHUVA FORTE)
- **Stat extra**: Nível (rain_level)
- **Detalhes**: Digital (molhado/seco), Bateria, RSSI, IP, Versão, Slot, Gateway
- **Sem Ciclo** — sensor read-only, sem timer/pulse

#### dht_gas
- **Hero**: Temperatura + Umidade lado a lado (cards grandes) + badge de gás (Seguro/Atenção/Vazamento)
- **Stat extra**: Temp (temperatura atual)
- **Detalhes**: Gás %, Alarme, DHT pin, Bateria, RSSI, IP, Versão, Slot, Gateway
- **Sem Ciclo** — sensor read-only, sem timer/pulse

#### repeater
- **Hero**: estatísticas de rede (Recebidos, Encaminhados, Clientes) em grid 2×2
- **Stats**: RX · TX · Clients (sem Mem — não relevante)
- **Sem Detalhes** collapsível — toda info já visível
- **Sidebar**: apenas Home + Configurações (sem Propriedades)
- Único cliente sem `/api/state` (usa `/api/status`)

### 2. Console module (console.h / console.cpp)

Extrair o manuseio serial inline para o mesmo padrão `console.h`/`console.cpp` usado no `onoff`/`lampada`/`gateway`:
- `console.printf()`, `console.loop()`, `console.telnet_available()`, `console.telnet_read()`
- Manter atalhos de teclado existentes + adicionar `h` para ajuda, `s` para status, `r` para restart

### 3. Endpoint /docs

Adicionar rota `/docs` com página HTML enxuta listando todos os endpoints da API (mesmo estilo do `onoff/docs`).

### 4. Configurações

Adicionar seção de Configurações na sidebar com:
- **Nome do dispositivo** (input text)
- **Reiniciar** (botão)
- Apenas estes — sem pinos configuráveis (chuva/dht_gas têm pinos fixos, repeater não tem GPIO de atuação)

### 5. Footer time

Usar `new Date().toLocaleTimeString('pt-BR', ...)` no JS (lado cliente) — sem depender de epoch/relógio sincronizado, igual feito no `onoff`.

## O que NÃO muda

- Lógica de sensores (leituras ADC/DHT, ESP-NOW send/recv, pareamento)
- Protocolo ESP-NOW (`espnow_protocol.h`)
- Configuração WiFi (WiFiManager mantido)
- EEPROM layout/endereços
- Nenhuma nova funcionalidade de sensor/atuação
- `platformio.ini` / build flags

## Arquivos por cliente

### chuva
| Arquivo | Ação |
|---------|------|
| `include/pages.h` | Reescrever com sidebar + stats + footer + docs |
| `include/console.h` | Novo (copiar do onoff) |
| `src/console.cpp` | Novo (copiar do onoff) |
| `src/main.cpp` | Adicionar `#include "console.h"`, extrair serial para console, adicionar rota `/docs`, adicionar /api/settings para device_name |
| `include/config.h` | Nenhuma mudança |

### dht_gas
| Arquivo | Ação |
|---------|------|
| `include/pages.h` | Reescrever com sidebar + stats + footer + docs |
| `include/console.h` | Novo (copiar do onoff) |
| `src/console.cpp` | Novo (copiar do onoff) |
| `src/main.cpp` | Adicionar `#include "console.h"`, extrair serial para console, adicionar rota `/docs`, adicionar /api/settings para device_name |
| `include/config.h` | Nenhuma mudança |
| `include/espnow_protocol.h` | Sincronizar versão (está desatualizada) |

### repeater
| Arquivo | Ação |
|---------|------|
| `include/pages.h` | Reescrever com sidebar (Home + Config) + stats + footer + docs |
| `include/console.h` | Novo |
| `src/console.cpp` | Novo |
| `src/main.cpp` | Adicionar console module, adicionar rota `/docs`, adicionar /api/settings para device_name |
| `include/config.h` | Nenhuma mudança |
