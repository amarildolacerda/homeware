# Rebranding: Homeware → AgriSense IoT

**Data**: 2026-07-24
**Objetivo**: Remover referências a "Arduino", "ESP8266", "ESP32" de nomes de pastas, device IDs e documentação — posicionar como plataforma profissional.

## Escopo: Full (pastas + docs + código interno)

## Arquitetura

| Atual | Novo | Função |
|-------|------|--------|
| `bridge/` | `server/` | Servidor de controle (add-on HA Python) |
| `gateway/` | `hub/` | Central de comunicação ESP-NOW |
| `clients/` | `nodes/` | Nós sensores/atuadores |

## Nomes das pastas de nós

| Atual | Novo | Descrição |
|-------|------|-----------|
| `clients/esp8266_lampada` | `nodes/lamp` | Relé ON/OFF + Alexa + repeater |
| `clients/esp8266_dht_gas` | `nodes/climate-gas` | DHT22 + MQ-2 |
| `clients/esp8266_pir` | `nodes/presence` | Sensor PIR |
| `clients/esp8266_pir_bat` | `nodes/presence-bat` | PIR bateria (deep sleep) |
| `clients/esp8266_chuva` | `nodes/rain` | Sensor de chuva |
| `clients/esp8266_onoff` | `nodes/switch` | Relé simples |
| `clients/esp8266_soil_moisture_bat` | `nodes/soil-moisture` | Umidade solo bateria |
| `clients/esp8266_repeater` | `nodes/extender` | Extensor de alcance |
| `clients/SPEC.md` | `nodes/SPEC.md` | Template de criação |

## Device ID

`esp8266_<chip_id>` → `agri_<chip_id>`

Afeta: registro no hub, dashboard (título/cabeçalho), HA entity IDs, logs.

## Código interno (shared/)

| Atual | Novo |
|-------|------|
| `HW_CHIP_ESP8266` | `HW_CHIP_ESP_1` |
| `HW_CHIP_ESP32` | `HW_CHIP_ESP_2` |
| `HW_CHIP_ESP32C3` | `HW_CHIP_ESP_3` |
| `chip_id()` | mantém (função interna) |
| `FW_VERSION` | mantém (constante interna) |
| `ESP8266WebServer` | mantém (já encapsulado em `MyWebServer`) |
| `ESP8266WiFi.h` | mantém (include técnico interno) |

## Envs PlatformIO

`esp8266_lamp_ota` → `lamp_ota`
`esp8266_dht_gas_ota` → `climate-gas_ota`
idem para todos os nodes.

## Shared submodule

Mudanças em `espnow_protocol.h` (enum `hw_chip_t`), `shared_config.h` (comentários), `platform.h` (comentários).

## Documentação

- README.md: reescrever tabela de clients, arquitetura, exemplos
- AGENTS.md: atualizar regras 1, 5, 13, 18, 21, 23, 24, seção de clients estáveis
- clients/SPEC.md → nodes/SPEC.md: atualizar template
- gateway/SPEC_ESPNOW.md: atualizar referências
- docs/ specs: atualizar referências

## Observações

- **Compatibilidade**: device ID novo (`agri_<chip_id>`) quebra pareamento de devices existentes em produção. Devices precisam re-registrar após update.
- **Ordem**: shared → hub → server → nodes (dependências bottom-up).
- **shared submodule**: commits no branch `dev` do repo `homeware_shared`, depois bump no homeware.
