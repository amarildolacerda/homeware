# Telnet Console para Gateway ESP8266

## Objetivo
Adicionar console remoto via telnet (TCP:23) no gateway para monitorar logs e enviar comandos quando o dispositivo não está acessível via USB serial.

## Arquitetura
- Módulo `console` unifica entrada/saída Serial + Telnet
- Usa biblioteca ESPTelnet para lidar com negociação telnet, reconnect e API Print
- Um único cliente telnet por vez

## Componentes
- `console.cpp` / `console.h` — init, loop, print wrapper
- `handle_serial()` refatorada → `handle_console(char c)` — mesma lógica, recebe char
- Serial continua single-key; telnet usa linha + Enter

## Mudanças
- `platformio.ini`: add `ESP Telnet` lib_deps
- `main.cpp`: `console_init()`, `console_loop()` no lugar de `handle_serial()`
- Todos os `Serial.printf/println` → `console_printf` em: web_server, espnow_handler, mqtt_client, sensor_registry, log_buffer
- Novos: `include/console.h`, `src/console.cpp`

## Trade-offs
- ~2-3KB flash extra pela biblioteca
- ~1-2KB heap a mais com cliente conectado
- Telnet em modo linha (Enter required) — mais simples que single-key remoto
