#pragma once
#include "espnow_node_protocol.h"
#include <string.h>
#include <stdio.h>

struct MockEspnowSend {
    uint8_t mac[6];
    uint8_t data[256];
    size_t len;
    bool sent = false;
};

extern MockEspnowSend g_mock_last_send;
extern int g_mock_send_count;
extern bool g_mock_espnow_init_ret;
extern bool g_mock_load_gateway_ret;

static inline bool espnow_client_init(const char* tag) {
    (void)tag;
    return g_mock_espnow_init_ret;
}

static inline bool espnow_client_add_peer(const uint8_t* mac, const char* tag) {
    (void)mac; (void)tag;
    return true;
}

static inline void espnow_save_gateway_mac(const uint8_t* mac, const char* tag) {
    (void)tag;
    memcpy(g_mock_last_send.mac, mac, 6);
}

static inline bool espnow_load_gateway_mac(uint8_t* mac_out, const char* tag) {
    (void)tag;
    if (g_mock_load_gateway_ret) {
        memset(mac_out, 0xAA, 6);
    }
    return g_mock_load_gateway_ret;
}

static inline int esp_now_init() { return g_mock_espnow_init_ret ? 0 : 1; }

static inline void esp_now_set_self_role(int) {}

static inline int esp_now_add_peer(uint8_t*, int, int, void*, size_t) { return 0; }

static inline int esp_now_del_peer(uint8_t*) { return 0; }

typedef void (*esp_now_send_cb_t)(uint8_t* mac, uint8_t status);
typedef void (*esp_now_recv_cb_t)(uint8_t* mac, uint8_t* data, uint8_t len);

static inline int esp_now_register_send_cb(esp_now_send_cb_t cb) { (void)cb; return 0; }

static inline int esp_now_register_recv_cb(esp_now_recv_cb_t cb) { (void)cb; return 0; }

static inline int espnow_send_wrapper(const uint8_t* mac, const uint8_t* data, size_t len, const char* tag) {
    (void)tag;
    g_mock_send_count++;
    memcpy(g_mock_last_send.mac, mac, 6);
    g_mock_last_send.len = len < 256 ? len : 256;
    memcpy(g_mock_last_send.data, data, g_mock_last_send.len);
    g_mock_last_send.sent = true;
    return 0;
}
