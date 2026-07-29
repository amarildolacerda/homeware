#include <Arduino.h>
#include "platform.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_registry.h"
#include "radio_manager.h"
#include "mqtt_client.h"
#ifdef HABILITA_ESPNOW
#include "espnow_handler.h"
#endif
#include "web_server.h"
#include "common_ota.h"
#include "log_buffer.h"
#ifdef HABILITA_LORA
#include "lora_handler.h"
#include "lora_protocol.h"
#endif
#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)
#include "display_handler.h"
#endif
#include "common_console.h"
#include "device_router.h"

static const char *TAG = PLATFORM_PREFIX "_gateway";

static unsigned long s_start_time = 0;
static unsigned long s_last_telemetry = 0;
static bool s_ntp_synced = false;
static unsigned long s_last_ntp_retry = 0;
static time_t s_ntp_epoch = 0;
static unsigned long s_last_time_sync = 0;
static time_t s_browser_epoch = 0;

#ifdef HABILITA_ESPNOW
static EspnowHandler s_espnow;
#endif
#ifdef HABILITA_LORA
static LoraHandler s_lora;
#define LORA_PENDING_STATE_MAX 5
static uint8_t s_lora_pending_state_slots[LORA_PENDING_STATE_MAX];
static int s_lora_pending_state_head = 0;
static int s_lora_pending_state_tail = 0;
#endif

RadioManager s_radio_mgr;

void print_help() {
    console.println("\n=== Comandos ===");
    console.println("  h/?  - Esta ajuda");
    console.println("  l    - Listar sensores pareados");
    console.printf("  p    - Iniciar modo pareamento (%us)\n", PAIRING_WINDOW_MS / 1000);
    console.println("  c    - Limpar TODOS os sensores");
    console.println("  r    - Reiniciar");
    console.println("  b    - Publicar todos os sensores via MQTT");
    console.println("  s    - Status do gateway");
    console.println("  w    - Forçar portal WiFi");
    console.println("================\n");
}

void handle_console(char c) {
    switch (c) {
        case 'h':
        case 'H':
        case '?':
            print_help();
            break;
            
        case 'l':
        case 'L':
            sensor_registry_print_all();
            break;
            
        case 'p':
        case 'P':
            if (s_radio_mgr.any_start_pairing()) {
                console.println("Modo pareamento iniciado. LED piscando...");
            } else {
                console.println("Falha: máximo de sensores atingido ou já em pareamento");
            }
            break;
            
        case 'c':
        case 'C':
            sensor_registry_clear_all();
            console.println("Todos os sensores removidos");
            break;
            
        case 'r':
        case 'R':
            log_add("warn", "Reiniciando...");
            console.println("Reiniciando...");
            delay(100);
            ESP.restart();
            break;
            
        case 'b':
        case 'B':
            mqtt_client_publish_all();
            break;
            
        case 's':
        case 'S': {
            console.printf("\n=== Status Gateway ===\n");
            console.printf("Device ID: %s\n", get_gateway_device_id());
            uint8_t sta_mac[6];
            WiFi.macAddress(sta_mac);
            char mac_buf[18];
            mac_to_str(sta_mac, mac_buf, sizeof(mac_buf));
            console.printf("MAC: %s\n", mac_buf);
            console.printf("FW: %s\n", FW_VERSION);
            console.printf("Uptime: %lu s\n", millis() / 1000);
            console.printf("NTP: %s\n", s_ntp_synced ? "sincronizado" : "aguardando...");
            console.printf("Sensores: %d pareados, %d online\n", 
                          sensor_registry_count_paired(), sensor_registry_count_online());
            console.printf("Radio RX=%lu ACK=%lu CRC_ERR=%lu\n",
                          s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count(), s_radio_mgr.total_crc_errors());
            console.printf("MQTT: %s:%d (%s)\n", 
                          mqtt_client_get_host(), mqtt_client_get_port(),
                          mqtt_client_is_connected() ? "conectado" : "desconectado");
            console.printf("WiFi: http://%s ch=%d (RSSI: %d dBm)\n",
                          WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "desconectado",
                          WiFi.channel(), WiFi.RSSI());
            console.printf("Pareamento: %s\n", s_radio_mgr.any_pairing_active() ? "ATIVO" : "inativo");
            console.printf("========================\n\n");
            break;
        }
            
        case 'w':
        case 'W':
            console.println("Forçando portal WiFi...");
            web_server_wifi_setup(true);
            break;
    }
}

#ifdef HABILITA_LORA
static void queue_bridge_state(int slot) {
    int next = (s_lora_pending_state_head + 1) % LORA_PENDING_STATE_MAX;
    if (next == s_lora_pending_state_tail) return;
    s_lora_pending_state_slots[s_lora_pending_state_head] = slot;
    s_lora_pending_state_head = next;
}

static void lora_process_bridge_queue(void) {
    while (s_lora_pending_state_tail != s_lora_pending_state_head) {
        int slot = s_lora_pending_state_slots[s_lora_pending_state_tail];
        s_lora_pending_state_tail = (s_lora_pending_state_tail + 1) % LORA_PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected()) {
            mqtt_client_publish_state(s);
        }
    }
}

static void lora_rx_cb(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    if (len < LORA_HEADER_SIZE) return;
    const lora_frame_t* frame = (const lora_frame_t*)data;

    int slot = sensor_registry_find_by_mac(frame->sensor_id);

    switch (frame->msg_type) {
        case LORA_MSG_PAIR_REQUEST: {
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot < 0) return;
                uint8_t sensor_type = frame->payload_len > 0 ? frame->payload[0] : 0;
                char dev_name[17] = {0};
                if (len >= (int)sizeof(lora_pair_request_t)) {
                    const lora_pair_request_t *req = (const lora_pair_request_t*)data;
                    strncpy(dev_name, req->device_name, sizeof(dev_name) - 1);
                }
                sensor_registry_add(frame->sensor_id, sensor_type, slot, dev_name, HW_CHIP_UNKNOWN, RADIO_LORA);
            }
            lora_pair_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.msg_type = LORA_MSG_PAIR_RESPONSE;
            resp.sequence = frame->sequence;
            memcpy(resp.sensor_id, frame->sensor_id, 6);
            resp.payload_len = 1;
            resp.assigned_slot = slot;
            s_lora.send((const uint8_t*)&resp, sizeof(resp));
            break;
        }
        case LORA_MSG_SENSOR_DATA: {
            if (slot >= 0) {
                espnow_header_t hdr;
                memset(&hdr, 0, sizeof(hdr));
                hdr.sequence = frame->sequence;
                hdr.rssi = rssi;
                hdr.payload_len = frame->payload_len;
                sensor_registry_update_state(slot, &hdr, frame->payload, frame->payload_len);
                queue_bridge_state(slot);
            }
            break;
        }
        case LORA_MSG_HEARTBEAT: {
            if (slot >= 0) {
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s) {
                    s->last_seen = millis();
                    s->online = true;
                }
            }
            break;
        }
    }
}
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);
    s_start_time = millis();
    
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, HIGH);
    pinMode(PAIR_BUTTON_GPIO, INPUT_PULLUP);
    
    console.printf("\n");
    console.printf("============================================\n");
#ifdef HABILITA_ESPNOW
    console.printf("  " PLATFORM_PREFIX " ESP-NOW Gateway %s\n", FW_VERSION);
#elif defined(HABILITA_LORA)
    console.printf("  " PLATFORM_PREFIX " LoRa Gateway %s\n", FW_VERSION);
#else
    console.printf("  " PLATFORM_PREFIX " Gateway %s\n", FW_VERSION);
#endif
    console.printf("  Device: %s\n", get_gateway_device_id());
    console.printf("============================================\n");
    
    sensor_registry_init();
    mqtt_client_load_config();
    mqtt_client_generate_device_ids();

    if (!web_server_wifi_setup(false)) {
        console.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }
    
    ota_setup(get_gateway_device_id());
    console.begin();
    {
        char banner[48];
        snprintf(banner, sizeof(banner), PLATFORM_PREFIX " Gateway %s", FW_VERSION);
        console.set_banner(banner);
    }
#ifdef HABILITA_ESPNOW
    s_espnow.set_rx_callback(nullptr, nullptr);
    s_radio_mgr.add_radio(RADIO_ESPNOW, &s_espnow);
#endif
#ifdef HABILITA_LORA
    s_lora.set_rx_callback(lora_rx_cb, nullptr);
    s_radio_mgr.add_radio(RADIO_LORA, &s_lora);
#endif
    s_radio_mgr.init_all();
#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)
    display_handler_init();
#endif
    s_radio_mgr.all_announce();
    log_buffer_init();
    
    configTime(0, 0, "162.159.200.123", "216.239.35.0");
    console.printf("[%s] NTP: 162.159.200.123 (cloudflare), non-blocking sync\n", TAG);
    
    mqtt_client_connect();
    
    console.printf("============================================\n");
    console.printf("  Pronto! 'h' para ajuda\n");
    console.printf("  Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
    console.printf("  Telnet: %s:23\n", WiFi.localIP().toString().c_str());
    console.printf("============================================\n\n");
}

void loop() {
    console.loop();
    
    if (Serial.available() > 0) {
        handle_console(Serial.read());
    }
    if (console.telnet_available() > 0) {
        char c = console.telnet_read();
        handle_console(c);
    }
    
    static unsigned long last_button_check = 0;
    static unsigned long press_start = 0;
    if (millis() - last_button_check > 50) {
        last_button_check = millis();
        if (digitalRead(PAIR_BUTTON_GPIO) == LOW) {
            if (press_start == 0) press_start = millis();
            else if (millis() - press_start > 3000) {
                if (!s_radio_mgr.any_pairing_active()) {
                    s_radio_mgr.any_start_pairing();
                }
                press_start = 0;
            }
        } else {
            press_start = 0;
        }
    }
    
    ota_handle();
    web_server_loop();
    web_server_maintain_wifi();
    s_radio_mgr.loop_all();
#ifdef HABILITA_LORA
    lora_process_bridge_queue();
#endif
#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)
    display_handler_loop();
#endif
    mqtt_client_loop();

    /* LED pisca durante o modo de pareamento */
    {
        static unsigned long s_led_toggle = 0;
        static bool s_led_state = false;
        if (s_radio_mgr.any_pairing_active()) {
            if (millis() - s_led_toggle > 300) {
                s_led_toggle = millis();
                s_led_state = !s_led_state;
                digitalWrite(STATUS_LED_GPIO, s_led_state ? LOW : HIGH);
            }
        }
    }
    
    unsigned long now = millis();
    
    if (now - s_last_telemetry > 30000) {
        s_last_telemetry = now;
        console.printf("[%s] Uptime=%lus RX=%lu ACK=%lu Paired=%d Online=%d MQTT=%d\n",
                      TAG, now / 1000, s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count(),
                      sensor_registry_count_paired(), sensor_registry_count_online(),
                      mqtt_client_is_connected());

    }

    if (!s_ntp_synced && millis() - s_last_ntp_retry > NTP_RETRY_INTERVAL_MS) {
        s_last_ntp_retry = millis();
        s_ntp_epoch = time(nullptr);
        if (s_ntp_epoch > 100000) {
            s_ntp_synced = true;
            struct tm *ti = localtime(&s_ntp_epoch);
            console.printf("[%s] NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                TAG, ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                ti->tm_hour, ti->tm_min, ti->tm_sec);
        } else {
            console.printf("[%s] NTP sync failed, will retry in %d min\n",
                TAG, NTP_RETRY_INTERVAL_MS / 60000);
        }
    }

    /* Update s_ntp_epoch every loop tick while synced */
    if (s_ntp_synced) {
        s_ntp_epoch = time(nullptr);
    }

    if (s_ntp_synced && millis() - s_last_time_sync > TIME_SYNC_INTERVAL_MS) {
        s_last_time_sync = millis();
        s_radio_mgr.all_broadcast_time_sync((uint32_t)s_ntp_epoch);
    }
    
    delay(1);
}

bool gateway_ntp_synced() { return s_ntp_synced; }
time_t gateway_ntp_epoch() { return s_ntp_synced ? s_ntp_epoch : s_browser_epoch; }

void gateway_set_browser_epoch(time_t epoch) {
    if (epoch > 100000 && !s_ntp_synced) s_browser_epoch = epoch;
}