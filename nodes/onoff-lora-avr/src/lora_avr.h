#pragma once

#include <Arduino.h>
#include "../include/lora_avr.h"
#include "../include/config.h"

// Initialize SoftwareSerial and bridge
void lora_init(const uint8_t *my_mac, const char *device_name);

// Send a raw frame
bool lora_send_frame(const uint8_t *data, uint8_t len);

// Convenience senders
bool lora_send_pair_request(uint8_t sensor_type);
bool lora_send_sensor_data(uint8_t relay_state);
bool lora_send_heartbeat();

// Non-blocking loop: check for received commands
typedef void (*lora_command_callback_t)(uint8_t slot, uint8_t command);
void lora_set_command_callback(lora_command_callback_t cb);

void lora_loop();

// State
bool lora_is_paired();
uint8_t lora_get_slot();
int8_t lora_get_last_rssi();
uint32_t lora_rx_count();
uint32_t lora_tx_count();
