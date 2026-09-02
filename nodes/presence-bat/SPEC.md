# Spec: Presence-BAT (Deep Sleep) - Estrada/Solar

Baseado em proposta técnica para nó de presença de baixíssimo consumo.

## Visão Geral
Dispositivo de sinalização bateria + solar para detectar passagem de veículos/pessoas em pontos isolados (estradas internas, porteiras) e transmitir evento instantaneamente via ESP-NOW para o Hub central. Opera estritamente sob demanda com Deep Sleep (~10-15µA).

## Requisitos de Hardware
- MCU: ESP32-C3/S3 (baixo consumo, boot rápido) - RTC GPIO para wakeup
- Sensor: PIR Externo/Industrial 12V via Step-Up chaveado ou AM312/SR602 3.3V (lente imune a pequenos animais)
- Alimentação: 18650 3.7V + Painel Solar 5V 1-2W + TP4056
- Wakeup: OUT do PIR -> RTC GPIO com EXT0 HIGH

## Arquitetura de Energia (Deep Sleep)
```
[DEEP SLEEP ~10µA] -> (PIR HIGH) -> [Wakeup EXT0] -> [Init ESP-NOW rápido sem WiFi] -> [Envio Payload] -> [ Aguarda callback 50ms ] -> [Deep Sleep]
```
- Sem inicializar WiFi completo, apenas ESP-NOW
- Telnet suspende sleep (console.telnet_connected()), disconnect -> restart, comando `p` re-pareia

## Payload (homeware_shared)
Leve, compatível com `espnow_protocol.h`:
```json
{
  "node_id": "presence_estrada_01",
  "type": "presence",
  "state": 1,
  "battery": 4.12,
  "v_reason": "ext0_wakeup"
}
```
Mapear para `payload_motion_t` + `battery_pct` + `ip`/`heap` existentes; `v_reason` pode ir em campo extra ou log.

## Firmware Esqueleto
```cpp
#define PIR_PIN GPIO_NUM_33 // RTC
#define BATT_SENSE_PIN 34
uint8_t hubMac[] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
void goToSleep(){ esp_sleep_enable_ext0_wakeup(PIR_PIN, 1); esp_deep_sleep_start(); }
void OnDataSent(const uint8_t *mac, esp_now_send_status_t s){ goToSleep(); }
void setup(){
  esp_sleep_wakeup_cause_t r = esp_sleep_get_wakeup_cause();
  if (r != ESP_SLEEP_WAKEUP_EXT0) { pinMode(PIR_PIN, INPUT); goToSleep(); return; }
  WiFi.mode(WIFI_STA); esp_now_init();
  esp_now_register_send_cb(OnDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, hubMac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) goToSleep();
  float batteryV = (analogRead(BATT_SENSE_PIN) * 3.3 / 4095.0) * 2;
  // usar homeware_shared: payload_motion_t + header
  // exemplo AgriSenseMessage -> mapear para espnow_protocol
  payload_motion_t pl = {.motion_state=1, .occupancy_duration=0};
  esp_now_send(hubMac, (uint8_t*)&pl, sizeof(pl)); // header via homeware_shared
  // fluxo fecha em OnDataSent <50ms
}
void loop(){ /* nunca executa devido ao Deep Sleep */ }
```

## Integração com Hub e Home Assistant
- Tópico de Estado: `homeassistant/binary_sensor/<id>/state` → `ON` (via `mqtt_client_publish_state()` no hub)
- Discovery: Server lê `type=presence` e cria `binary_sensor` `motion`/`occupancy`
- Automação de Retorno (Off): nó dorme sem enviar OFF; usar `off_delay: 30` no HA ou Server Python para resetar para `OFF`

## Regras Herdadas
- Device ID dinâmico `presence_<chip_id>`, nome configurável via WiFiManager
- Dashboard compacto com Detalhes colapsável (quando acordado via telnet)
- Versão FW_VERSION igual tag
- Loop non-blocking quando acordado; deep sleep quando ocioso
- `WIFI_NONE_SLEEP` não se aplica (deep sleep)

## Próximos Passos
- Criar `nodes/presence-bat/` a partir de `nodes/presence/` com flags `-D PRESENCE_BAT` + deep sleep
- Reaproveitar `shared/` para protocolo, adicionar `presence-bat` em `platformio.ini` env `esp32c3`
