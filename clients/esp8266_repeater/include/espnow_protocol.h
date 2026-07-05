#pragma once

#include <stdint.h>
#include <string.h>
#include <Arduino.h>

#define ESPNOW_PROTOCOL_VERSION 1

/* Protocol boardcast ID (gateway) */
#define BROADCAST_ID 0xFFFF

/* Max sensor name length */
#define SENSOR_NAME_MAX 32

/* Sensor type definitions */
#define SENSOR_TYPE_RAIN      1
#define SENSOR_TYPE_DHT       2
#define SENSOR_TYPE_GAS       3
#define SENSOR_TYPE_LIGHT     4
#define SENSOR_TYPE_SOIL      5
#define SENSOR_TYPE_TEMP_SOIL 6
#define SENSOR_TYPE_ONOFF     8

/* Payload for generic sensor */
typedef struct __attribute__((packed))
{
    float value;
} payload_float_t;

/* Payload for DHT sensor */
typedef struct __attribute__((packed))
{
    float temperature;
    float humidity;
} payload_dht_t;

/* Payload for ONOFF sensor */
typedef struct __attribute__((packed))
{
    uint8_t state;
} payload_onoff_t;

/* Payload for gas sensor */
typedef struct __attribute__((packed))
{
    uint16_t mq2;
    uint16_t mq135;
    uint16_t mq7;
} payload_gas_t;

/* Payload for soil sensor */
typedef struct __attribute__((packed))
{
    uint16_t moisture;
    float temperature;
} payload_soil_t;

/* Payload for light sensor */
typedef struct __attribute__((packed))
{
    uint16_t lux;
} payload_light_t;

/* Message types */
#define ESPNOW_MSG_SENSOR_DATA      0x01
#define ESPNOW_MSG_PAIR_REQUEST     0x02
#define ESPNOW_MSG_PAIR_RESPONSE    0x03
#define ESPNOW_MSG_HEARTBEAT        0x04
#define ESPNOW_MSG_ACK              0x05
#define ESPNOW_MSG_COMMAND          0x06
#define ESPNOW_MSG_CONFIG           0x07

/* Pair status codes */
#define PAIR_STATUS_OK              0x00
#define PAIR_STATUS_DENIED          0x01
#define PAIR_STATUS_FULL            0x02
#define PAIR_STATUS_ERROR           0xFF

/* Max payload size */
#define ESPNOW_PAYLOAD_MAX 200

/* Header for all ESPNOW messages */
typedef struct __attribute__((packed))
{
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_mac[6];
    uint16_t sensor_type;
    uint8_t  battery_pct;
    int16_t  rssi;
    uint16_t payload_len;
    uint8_t  payload[ESPNOW_PAYLOAD_MAX];
} espnow_header_t;

/* Pair request message */
typedef struct __attribute__((packed))
{
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  sensor_mac[6];
    uint16_t sensor_type;
    uint8_t  firmware_version[4];
    char     device_name[SENSOR_NAME_MAX];
} espnow_pair_request_t;

/* Pair response message */
typedef struct __attribute__((packed))
{
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  status;
    uint16_t assigned_slot;
} espnow_pair_response_t;

/* ACK message */
typedef struct __attribute__((packed))
{
    uint8_t  msg_type;
    uint16_t sequence;
    uint8_t  status;
} espnow_ack_t;

/* Utility functions */
static inline void mac_copy(uint8_t *dst, const uint8_t *src)
{
    memcpy(dst, src, 6);
}

static inline int mac_is_equal(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, 6) == 0;
}

static inline void mac_to_str(const uint8_t *mac, char *str, size_t len)
{
    snprintf(str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
