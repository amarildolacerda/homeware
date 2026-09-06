# Telegram - Alertas e Monitoramento (Hub v1.2.26+)

> Fonte única: `hub/src/telegram_bot.cpp:463` `check_alerts()` + `hub/include/config_store.h:14` `hub/SPEC_telegram.md:641`

## 1. Visão Geral
Monitora saúde do hub e sensores a cada 30s (`hub/src/telegram_bot.cpp:467`) e envia via `UniversalTelegramBot` (`hub/platformio.ini` `TELEGRAM_ENABLED`). Requer `WiFi` + `NTP` ou `browser epoch` (`hub/src/main.cpp:492` `gateway_ntp_epoch()`). Ignorado se `free_heap<15KB`.

Configuração em `http://<hub-ip>/settings → Telegram` (`hub/include/pages.h:1154`) persiste em `LittleFS /telegram.json` (`hub/src/config_store.cpp:14`). Bitmasks `alerts_level 0x0F` e `alerts_type 0x03FF` (`hub/src/web_server.cpp:623`).

## 2. Tipos de Alerta (10 bits `alerts_type`)

| Bit | Tipo | Nível SPEC | Condição `telegram_bot.cpp` | Throttle | Emoji |
|---|---|---|---|---|---|
|0|`gas`|ALERT `⚠️`| `DHT_GAS/GAS gas_level>400` |60s `GAS_MS`|⚠️ `Gás 380ppm em X`|
|1|`smoke`|CRITICAL `🔴`| `alarm==true` (mesmo tipo) |0|🔴 `Fumaça em X`|
|2|`offline`|ALERT `⚠️`| `!online && millis-last_seen>5min` |5min|⚠️ `offline há 12min`|
|3|`reconnect`|INFO `🟢`| transição `!prev_online→online` |5min|🟢 `reconectou`|
|4|`battery`|CRITICAL/WARNING| `<10%` CRITICAL 0 throttle / `<20%` WARNING 1h|0 /1h|🔴 `CRÍTICA 8%` / 🟡 `baixa 18%`|
|5|`temperature`|ALERT `⚠️`| `TEMP_HUM/DHT_GAS temp>35°C ou <10°C` |5min|⚠️ `Temperatura alta 42°C`|
|6|`humidity`|ALERT `⚠️`| `hum>85% ou <30%` |5min|⚠️ `Umidade 90% alta`|
|7|`rssi`|WARNING `🟡`| `last_rssi<-80` |1h|🟡 `RSSI -85dBm`|
|8|`heap`|WARNING `🟡`| `ESP.getFreeHeap()<50KB` |1h|🟡 `Heap baixo 45KB`|
|9|`daily_report`|INFO `🟢`| `08:00` local `gateway_ntp_epoch()` |24h|🟢 `Resumo diário: 3/5 online`|
|—|`mqtt`|ALERT `⚠️`| `!mqtt_client_is_connected() >5min` |5min (reuse offline)|⚠️ `MQTT offline há 7min`|

Todos respeitam `is_alert_type_enabled(bit)` + `is_alert_level_enabled(level)` (`hub/src/telegram_bot.cpp:95`).

## 3. Níveis `alerts_level`
`0:critical(🔴)` `1:alert(⚠️)` `2:warning(🟡)` `3:info(🟢)` — desmarcar no modal desativa classe inteira.

## 4. Throttling
`THROTTLE_CRITICAL 0` / `ALERT 5min` / `WARNING 1h` / `INFO 5min` + overrides `GAS 1min` `OFFLINE 5min` `BATTERY 1h` `DAILY 24h` (`hub/src/telegram_bot.cpp:44`). Evita spam com `Telegram limit 30msg/s`.

## 5. Monitoramento Contínuo
- **Polling** `telegram_bot_loop()` a cada `poll_interval 2s` (`hub/src/telegram_bot.cpp:501`)
- **Watchdog RX** `hub/src/main.cpp:386` `HUB_RX_WATCHDOG_MS 5min` sem `total_rx` → `ESP.restart()` (log `warn`, sem telegram pré-reboot)
- **Agenda restart** `hub/src/main.cpp:492` `/agenda` `00:00` — checa a cada 15s, `log_add warn` antes de `restart`
- **TLS** `WiFiClientSecure` `~30KB` RAM; pausa se `free_heap<15KB`

## 6. Comandos Telegram
`/start|/help` `/status` `/list` (bola `🟢 ON` / `⚫ OFF` para lamp `hub/src/telegram_bot.cpp:166`) `/on <slot|all>` `/off <slot|all>` + teclado reply `[ON]0 Entrada` (`hub/src/telegram_bot.cpp:211`).

## 7. Dashboard & API
- `GET /api/config/telegram` / `POST /api/config/telegram` (`hub/src/web_server.cpp:623`)
- `GET /api/info` inclui `telegram_enabled`, `mqtt_connected`, `free_heap`, `epoch`
- `POST /api/logs/clear` limpa `log_buffer` (logs não afetam alertas)

## 8. Exemplo Fluxo
`Node 0 offline 6min → ⚠️ offline` → `reconnect → 🟢 reconectou` → `gas 420ppm → ⚠️ Gás` → `08:00 → 🟢 Resumo 5/5 online`

## 9. Limitações & Futuro
- `OTA` e `config altered` ainda sem alerta (SPEC 5.2 `WARNING`)
- `WiFi STA offline` e `NTP unsynced >2h` planejados (fora SPEC, útil p/ `agenda`)
- Inline keyboards e `/alerts on/off` pendentes `SPEC_telegram.md:124`

Referência SPEC completa: `hub/SPEC_telegram.md`
