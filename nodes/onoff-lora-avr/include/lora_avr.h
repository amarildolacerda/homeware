#pragma once

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

// ── UART Protocol for ATMega168 Bridge ──
// TX: 'T' + len_hi + len_lo + [data...]
// RX enable: 'R'
// Status: '?' → 'T'(ready), 'B'(busy)
// Response: 'D' + len_hi + len_lo + [data...] (received packet)
//           'T' (TX complete)
//           'E' (error)

#define BRIDGE_CMD_TX      'T'
#define BRIDGE_CMD_RX_EN   'R'
#define BRIDGE_CMD_STATUS  '?'
#define BRIDGE_RSP_TX_OK   'T'
#define BRIDGE_RSP_DATA    'D'
#define BRIDGE_RSP_ERROR   'E'
#define BRIDGE_RSP_READY   'T'
#define BRIDGE_RSP_BUSY    'B'
