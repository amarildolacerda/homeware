#pragma once
#include <Arduino.h>

#define DEVICE_NAME "AgriSense Acquiry Bat"

#define STATE_UPDATE_INTERVAL 5000
#define HEARTBEAT_INTERVAL 60000

#define ACQUERY_PIN 4
#define CFG_BUTTON_PIN 0
#define LED_PIN 2

#define LED_BLINK_SEND_MS 100

#define ESPNOW_CHANNEL 1
#define ESPNOW_STARTUP_WAIT_MS 10000
#define ESPNOW_PAIR_INTERVAL_MS 3000
#define ESPNOW_MAX_PAIR_ATTEMPTS 10
#define ESPNOW_PAIR_TIMEOUT_MS 30000

#define WIFI_CONFIG_PORTAL_SSID "AgriSense_Acquiry_Bat"
#define WIFI_CONFIG_PORTAL_PASS "password123"
#define WIFI_CONFIG_PORTAL_TIMEOUT_S 120
#define WIFI_CONNECT_TIMEOUT_MS 30000
// ESP-NOW puro: timeout curto só para herdar o canal do AP (sem exigir IP/portal)
#define WIFI_INHERIT_TIMEOUT_MS 3000

#define DEEP_SLEEP_INTERVAL_DEFAULT 60
#define DEEP_SLEEP_INTERVAL_MIN 1
#define DEEP_SLEEP_INTERVAL_MAX 3600
#define DEEP_SLEEP_RETRY_INTERVAL 10

#define ACQUIRY_HOLD_TIME_MS 30000

// Labels de estado exibidos no card do hub (payload_int_t.label, máx. 11 chars + '\0')
#define ACQ_LABEL_ON  "aberto"
#define ACQ_LABEL_OFF "fechado"

// Bateria via ADC — DESABILITADO por padrão (BATTERY_ADC_PIN < 0) porque o
// divisor de tensão da placa final ainda não está definido. Sem ADC a leitura
// reporta 100% (sem simulação/drift). Para habilitar, definir o pino analógico
// (ESP8266: A0; no ESP32 ajustar também BATTERY_ADC_MAX para 4095) e a razão
// do divisor; a tensão da bateria é mapeada na faixa 3.3V (0%)..4.2V (100%).
#define BATTERY_ADC_PIN -1
#define BATTERY_ADC_MIN 0
#define BATTERY_ADC_MAX 1023
#define BATTERY_DIVIDER_RATIO 2.0f