# Unificação de Flags de Rádio

**Data:** 2026-07-31
**Status:** Aprovado

## Problema

Três flags com semântica sobreposta e redundante:

| Flag | Onde | Semântica atual |
|------|------|-----------------|
| `LORA_DEVICE` | shared/, nodes/ | "Exclui APIs ESP-NOW" (não "usa LoRa") |
| `HABILITA_LORA` | hub/ | "Hub inclui suporte LoRa" |
| `HABILITA_ESPNOW` | hub/ | "Hub inclui suporte ESP-NOW" |

- `LORA_DEVICE` e `HABILITA_LORA` são redundantes (quase sempre juntos)
- `LORA_DEVICE` no shared tem semântica diferente do nome
- Mix de idiomas: `HABILITA_` (PT) + `ESPNOW_`/`LORA_` (EN)
- Guard complexo: `#if !defined(LORA_DEVICE) || defined(HABILITA_ESPNOW)`

## Solução

Unificar em dois flags limpos com semântica clara:

| Antes | Depois | Semântica |
|-------|--------|-----------|
| `LORA_DEVICE` | `LORA_ENABLED` | "Este device usa LoRa" |
| `HABILITA_LORA` | `LORA_ENABLED` | "Hub inclui suporte LoRa" |
| `HABILITA_ESPNOW` | `ESPNOW_ENABLED` | "Hub inclui suporte ESP-NOW" |

### Guard no shared

```cpp
// Antes:
#if !defined(LORA_DEVICE) || defined(HABILITA_ESPNOW)
#include <esp_now.h>
#endif

// Depois:
#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)
#include <esp_now.h>
#endif
```

Semântica: inclui APIs ESP-NOW quando `ESPNOW_ENABLED` está definido OU quando nenhum rádio está habilitado (fallback para ESP-NOW puro).

## Mapeamento por arquivo

### platformio.ini (hub/)

| Env | Antes | Depois |
|-----|-------|--------|
| `hub_espnow` | `HABILITA_ESPNOW` | `ESPNOW_ENABLED` |
| `hub_32_lora` | `HABILITA_LORA` + `LORA_DEVICE` | `LORA_ENABLED` |
| `hub_32_lora_heltec` | `HABILITA_LORA` + `LORA_DEVICE` + `HABILITA_ESPNOW` | `LORA_ENABLED` + `ESPNOW_ENABLED` |

### platformio.ini (nodes/)

| Env | Antes | Depois |
|-----|-------|--------|
| `onoff-lora` | `LORA_DEVICE` | `LORA_ENABLED` |

### Source code (hub/)

- `src/main.cpp`: `HABILITA_ESPNOW` → `ESPNOW_ENABLED`, `HABILITA_LORA` → `LORA_ENABLED`
- `src/espnow_handler.cpp`: `HABILITA_ESPNOW` → `ESPNOW_ENABLED`
- `include/espnow_handler.h`: `HABILITA_ESPNOW` → `ESPNOW_ENABLED`
- `src/lora_handler.cpp`: `HABILITA_LORA` → `LORA_ENABLED`
- `include/pages.h`: `HABILITA_LORA` → `LORA_ENABLED`

### Source code (shared/)

- `src/espnow_protocol.h`: guards → `#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)`
- `src/common_espnow.h`: sem alteração (já usa `ARDUINO_ARCH_ESP32`)
- `src/lora_spi_radio.cpp`: `LORA_DEVICE` → `LORA_ENABLED`
- `src/espnow_node_protocol.cpp`: sem alteração

### Docs

- `hub/SPEC_LORA.md`: `HABILITA_LORA` → `LORA_ENABLED`
- `nodes/SPEC.md`: `LORA_DEVICE` → `LORA_ENABLED`

## Impacto

- **Risco baixo:** rename mecânico, sem mudança de lógica
- **Arquivos afetados:** ~15 (platformio.ini + source + docs)
- **Nodes não afetados:** todos os nodes ESP-NOW (não definem nenhum desses flags)
- **Nodes afetados:** apenas `onoff-lora` (LORA_DEVICE → LORA_ENABLED)

## Verificação

1. `pio run -d hub -e hub_32_lora_heltec` — deve compilar sem erros
2. `pio run -d hub -e hub_8266` — deve compilar (ESP-NOW puro)
3. `pio run -d nodes/onoff-lora` — deve compilar (LoRa puro)
4. Grep por `HABILITA_` e `LORA_DEVICE` — deve retornar 0 ocorrências em source/platformio
