#include <Arduino.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include "../include/config.h"
#include "../include/lora_avr.h"

// ── State ──
static bool s_relay = false;
static bool s_paired = false;
static uint8_t s_my_mac[6];
static char s_device_id[16];
static char s_device_name[32] = DEVICE_NAME;

// ── Timing ──
static unsigned long s_last_state_send = 0;
static unsigned long s_last_heartbeat = 0;
static unsigned long s_last_pair_attempt = 0;
static uint8_t s_pair_attempts = 0;
static unsigned long s_last_led_toggle = 0;
static bool s_led_state = false;

// ── EEPROM ──

static void relay_save() {
    EEPROM.write(EEPROM_RELAY_STATE, s_relay ? 1 : 0);
}

static void relay_load() {
    s_relay = (EEPROM.read(EEPROM_RELAY_STATE) == 1);
}

static void paired_save() {
    EEPROM.write(EEPROM_PAIRED_FLAG, 1);
}

static void paired_load() {
    s_paired = (EEPROM.read(EEPROM_PAIRED_FLAG) == 1);
}

static void mac_save() {
    for (uint8_t i = 0; i < 6; i++) {
        EEPROM.write(EEPROM_MY_MAC + i, s_my_mac[i]);
    }
}

static void mac_load() {
    for (uint8_t i = 0; i < 6; i++) {
        s_my_mac[i] = EEPROM.read(EEPROM_MY_MAC + i);
    }
}

// ── Device ID from ATmega328P signature ──

static void generate_device_id() {
    uint8_t sig0 = pgm_read_byte(0x00);
    uint8_t sig1 = pgm_read_byte(0x01);
    uint8_t sig2 = pgm_read_byte(0x02);

    s_my_mac[0] = sig0;
    s_my_mac[1] = sig1;
    s_my_mac[2] = sig2;
    s_my_mac[3] = 0x00;
    s_my_mac[4] = 0x00;
    s_my_mac[5] = 0x00;

    snprintf(s_device_id, sizeof(s_device_id), "avr_%02X%02X%02X", sig0, sig1, sig2);
}

// ── Relay ──

static void set_relay(bool state) {
    s_relay = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : !RELAY_ON);
    relay_save();
    s_last_state_send = 0;
    Serial.print("Relay: ");
    Serial.println(state ? "ON" : "OFF");
}

static void toggle_relay() {
    set_relay(!s_relay);
}

// ── LoRa Command Callback ──

static void on_lora_command(uint8_t slot, uint8_t command) {
    if (command == 0x01) {
        set_relay(true);
    } else if (command == 0x00) {
        set_relay(false);
    } else if (command == 0xFF) {
        Serial.println("Restart command received");
        delay(100);
        asm volatile("jmp 0x0000");
    }
}

// ── LED ──

static void led_update() {
    unsigned long now = millis();

    if (!s_paired) {
        if (now - s_last_led_toggle > LED_BLINK_PAIR_MS) {
            s_last_led_toggle = now;
            s_led_state = !s_led_state;
            digitalWrite(LED_PIN, s_led_state ? LOW : HIGH);
        }
    } else {
        digitalWrite(LED_PIN, s_relay ? LOW : HIGH);
    }
}

// ── Serial Console ──

static void print_help() {
    Serial.println("\n=== Comandos ===");
    Serial.println("  h/?  - Ajuda");
    Serial.println("  s    - Status");
    Serial.println("  l    - Alterna rele");
    Serial.println("  p    - Re-parear");
    Serial.println("  r    - Reiniciar");
    Serial.println("================\n");
}

static void print_status() {
    Serial.println("\n=== Status ===");
    Serial.print("  Device:  "); Serial.println(s_device_id);
    Serial.print("  Nome:    "); Serial.println(s_device_name);
    Serial.print("  Rele:    "); Serial.println(s_relay ? "ON" : "OFF");
    Serial.print("  Pareado: "); Serial.println(s_paired ? "Sim" : "Nao");
    Serial.print("  Slot:    "); Serial.println(s_paired ? "0" : "-");
    Serial.print("  RSSI:    "); Serial.print(lora_get_last_rssi()); Serial.println(" dBm (LoRa)");
    Serial.print("  Uptime:  "); Serial.print(millis() / 1000); Serial.println("s");
    Serial.print("  RX/TX:   "); Serial.print(lora_rx_count()); Serial.print(" / "); Serial.println(lora_tx_count());
    Serial.println("=============\n");
}

static void handle_serial(char c) {
    switch (c) {
        case 'h': case 'H': case '?':
            print_help();
            break;
        case 's': case 'S':
            print_status();
            break;
        case 'l': case 'L':
            toggle_relay();
            break;
        case 'p': case 'P':
            s_paired = false;
            s_pair_attempts = 0;
            s_last_pair_attempt = 0;
            paired_save();
            Serial.println("Re-pareando...");
            break;
        case 'r': case 'R':
            Serial.println("Reiniciando...");
            delay(100);
            asm volatile("jmp 0x0000");
            break;
    }
}

// ── Setup ──

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    Serial.println("\n\nLoRa AVR Switch starting...");

    generate_device_id();
    Serial.print("Device: ");
    Serial.println(s_device_id);

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    relay_load();
    paired_load();
    mac_load();

    bool mac_valid = false;
    for (uint8_t i = 0; i < 6; i++) {
        if (s_my_mac[i] != 0x00) { mac_valid = true; break; }
    }
    if (!mac_valid) {
        mac_save();
    }

    digitalWrite(RELAY_PIN, s_relay ? RELAY_ON : !RELAY_ON);
    Serial.print("Relay: ");
    Serial.println(s_relay ? "ON" : "OFF");

    lora_init(s_my_mac, s_device_name);
    lora_set_command_callback(on_lora_command);
    Serial.println("LoRa initialized");

    Serial.println("Ready! 'h' for help\n");
}

// ── Loop ──

void loop() {
    unsigned long now = millis();

    if (Serial.available()) {
        handle_serial(Serial.read());
    }

    lora_loop();

    if (!s_paired) {
        if (s_pair_attempts < PAIR_MAX_ATTEMPTS &&
            now - s_last_pair_attempt >= PAIR_INTERVAL_MS) {
            s_last_pair_attempt = now;
            s_pair_attempts++;
            Serial.print("Pair attempt ");
            Serial.print(s_pair_attempts);
            Serial.print("/");
            Serial.println(PAIR_MAX_ATTEMPTS);
            lora_send_pair_request(SENSOR_TYPE_ONOFF);
        }
    } else {
        if (now - s_last_state_send >= STATE_UPDATE_INTERVAL_MS) {
            s_last_state_send = now;
            lora_send_sensor_data(s_relay ? 1 : 0);
        }

        if (now - s_last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            s_last_heartbeat = now;
            lora_send_heartbeat();
        }
    }

    static unsigned long s_last_button_check = 0;
    if (now - s_last_button_check > BUTTON_DEBOUNCE_MS) {
        s_last_button_check = now;
        if (digitalRead(BUTTON_PIN) == LOW) {
            delay(BUTTON_DEBOUNCE_MS);
            if (digitalRead(BUTTON_PIN) == LOW) {
                while (digitalRead(BUTTON_PIN) == LOW) delay(10);
                toggle_relay();
            }
        }
    }

    led_update();

    delay(1);
}
