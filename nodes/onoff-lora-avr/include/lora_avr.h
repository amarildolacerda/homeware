#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// ── Message Types (must match shared/src/msg_type.h) ──
enum msg_type_t : uint8_t {
    MSG_SENSOR_DATA     = 0x01,
    MSG_PAIR_REQUEST    = 0x02,
    MSG_PAIR_RESPONSE   = 0x03,
    MSG_ACK             = 0x04,
    MSG_HEARTBEAT       = 0x05,
    MSG_COMMAND         = 0x07,
    MSG_NAK             = 0x0D,
};

// ── Sensor Types ──
#define SENSOR_TYPE_ONOFF 8

// ── Protocol Structs (packed, match shared/src/lora_protocol.h) ──
#pragma pack(push, 1)

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  payload[];
} lora_frame_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  assigned_slot;
} lora_pair_response_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  sensor_type;
    char     device_name[16];
} lora_pair_request_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_id[6];
    int8_t   rssi;
    uint8_t  payload_len;
    uint8_t  command;
} lora_command_t;

#pragma pack(pop)

#define LORA_HEADER_SIZE   11
#define LORA_MAX_PAYLOAD   200

// ── LoRa API ──

void lora_init(const uint8_t *my_mac, const char *device_name);
bool lora_send_frame(const uint8_t *data, uint8_t len);
bool lora_send_pair_request(uint8_t sensor_type);
bool lora_send_sensor_data(uint8_t relay_state);
bool lora_send_heartbeat();

typedef void (*lora_command_callback_t)(uint8_t slot, uint8_t command);
void lora_set_command_callback(lora_command_callback_t cb);

void lora_loop();

bool lora_is_paired();
uint8_t lora_get_slot();
int8_t lora_get_last_rssi();
uint32_t lora_rx_count();
uint32_t lora_tx_count();
