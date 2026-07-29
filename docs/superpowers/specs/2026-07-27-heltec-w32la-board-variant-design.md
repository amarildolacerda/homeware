# Heltec HTIT-W32LA Board Variant for LoRa Hub

## Problem

The hub LoRa suporta atualmente apenas a TTGO LORA32 T3 v1.6 (SX1278). O Heltec WiFi LoRa 32 V2 (HTIT-W32LA, SX1276) tem pinagem diferente para o rádio LoRa e o display OLED, impedindo seu uso com o mesmo firmware.

## Solução

Nova env `hub_32_lora_heltec` no PlatformIO com flag `-D HELTEC_W32LA`. Os pinos específicos de cada board são selecionados via `#ifdef` nos arquivos de config existentes, mantendo uma única base de código.

## Arquivos alterados

### `platformio.ini`
- Novo env `[env:hub_32_lora_heltec]` estendendo `hub_32`, adicionando `-D HELTEC_W32LA -D HABILITA_LORA -D HABILITA_DISPLAY_TTGO` e mesmas libs do env TTGO.

### `include/lora_config.h`
- `LORA_RST`: TTGO=23, Heltec=14
- `LORA_DIO0`: TTGO=-1, Heltec=26

### `include/display_config.h`
- `DISPLAY_SDA`: TTGO=21, Heltec=4
- `DISPLAY_SCL`: TTGO=22, Heltec=15
- `DISPLAY_RST`: TTGO=-1, Heltec=16

### `src/display_handler.cpp`
- Heltec precisa de um reset pulse no GPIO16 (`pinMode(16, OUTPUT); digitalWrite(16, LOW); delay(100); digitalWrite(16, HIGH);`) antes do `display.begin()`.

## Não alterado
- Protocolo LoRa (`lora_protocol.h`), handler (`lora_handler.cpp`), RadioInterface — idênticos
- Lógica de display (páginas, conteúdo) — idêntica
- Demais funcionalidades do hub (MQTT, REST, sensor_registry, etc.)
