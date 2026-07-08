#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "mqtt_client.h"
#include "web_server.h"
#include "ota.h"
#include "log_buffer.h"
#include "console.h"

static const char *TAG = "esp8266_gateway";

static unsigned long s_start_time = 0;
static unsigned long s_last_telemetry = 0;

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
            if (espnow_start_pairing()) {
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
            char mac_buf[18];
            mac_to_str(espnow_get_gateway_mac(), mac_buf, sizeof(mac_buf));
            console.printf("MAC: %s\n", mac_buf);
            console.printf("FW: %s\n", FW_VERSION);
            console.printf("Uptime: %lu s\n", millis() / 1000);
            console.printf("Sensores: %d pareados, %d online\n", 
                          sensor_registry_count_paired(), sensor_registry_count_online());
            console.printf("ESP-NOW: RX=%lu ACK=%lu CRC_ERR=%lu\n",
                          espnow_get_rx_count(), espnow_get_ack_count(), espnow_get_crc_errors());
            console.printf("MQTT: %s:%d (%s)\n", 
                          mqtt_client_get_host(), mqtt_client_get_port(),
                          mqtt_client_is_connected() ? "conectado" : "desconectado");
            console.printf("WiFi: %s ch=%d (RSSI: %d dBm)\n",
                          WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "desconectado",
                          WiFi.channel(), WiFi.RSSI());
            console.printf("Pareamento: %s\n", espnow_is_pairing() ? "ATIVO" : "inativo");
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    console.begin();
    s_start_time = millis();
    
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, HIGH);
    pinMode(PAIR_BUTTON_GPIO, INPUT_PULLUP);
    
    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  ESP8266 ESP-NOW Gateway %s\n", FW_VERSION);
    console.printf("  Device: %s\n", get_gateway_device_id());
    console.printf("============================================\n");
    
    sensor_registry_init();
    mqtt_client_load_config();
    mqtt_client_generate_device_ids();
    
    ota_init(get_gateway_device_id());
    
    if (!web_server_wifi_setup(false)) {
        console.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }
    
    espnow_handler_init();
    log_buffer_init();
    web_server_init();
    
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
                if (!espnow_is_pairing()) {
                    espnow_start_pairing();
                }
                press_start = 0;
            }
        } else {
            press_start = 0;
        }
    }
    
    web_server_loop();
    web_server_maintain_wifi();
    espnow_handler_loop();
    mqtt_client_loop();
    
    unsigned long now = millis();
    
    if (now - s_last_telemetry > 30000) {
        s_last_telemetry = now;
        console.printf("[%s] Uptime=%lus RX=%lu ACK=%lu Paired=%d Online=%d MQTT=%d\n",
                      TAG, now / 1000, espnow_get_rx_count(), espnow_get_ack_count(),
                      sensor_registry_count_paired(), sensor_registry_count_online(),
                      mqtt_client_is_connected());

    }
    
    delay(1);
}