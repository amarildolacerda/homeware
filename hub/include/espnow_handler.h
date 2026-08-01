#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include "radio_interface.h"
#include "espnow_protocol.h"
#include "sensor_registry.h"
#include <stdint.h>

#ifdef ESPNOW_ENABLED

class EspnowHandler : public RadioInterface {
public:
    EspnowHandler();
    ~EspnowHandler() {}

    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

    bool start_pairing() override;
    void stop_pairing() override;
    bool is_pairing() const override;
    unsigned long pairing_remaining_ms() const override;

    unsigned long get_rx_count() const override { return m_rx_count; }
    unsigned long get_ack_count() const override { return m_ack_count; }
    unsigned long get_crc_errors() const override { return m_crc_errors; }

    uint8_t* get_radio_mac() override { return m_gateway_mac; }
    void announce() override;
    void broadcast_time_sync(uint32_t epoch_seconds) override;

    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;

    void handle_rx(const uint8_t* mac, const uint8_t* data, int len);

public:
    static EspnowHandler* s_self;  // accessed from C-linkage callback

private:
    bool m_pairing_mode = false;
    unsigned long m_pairing_start = 0;
    uint8_t m_gateway_mac[6];
    unsigned long m_last_heartbeat = 0;
    unsigned long m_rx_count = 0;
    unsigned long m_ack_count = 0;
    unsigned long m_crc_errors = 0;

    static const uint8_t s_bcast_addr[6];

    struct PendingPair {
        bool active;
        uint8_t mac[6];
        uint8_t sensor_type;
        uint16_t sequence;
        uint8_t client_chip;
        char name[32];
    };
    static const int PENDING_PAIR_MAX = 5;
    PendingPair m_pending_pairs[PENDING_PAIR_MAX];

    static const int PENDING_STATE_MAX = 5;
    uint8_t m_pending_state_slots[PENDING_STATE_MAX];
    int m_pending_state_head = 0;
    int m_pending_state_tail = 0;

    uint16_t m_time_sync_sequence = 0;

    void queue_bridge_state(int slot);
    void process_bridge_queue();
    void send_ack(const uint8_t* mac, uint16_t sequence, uint8_t status, uint8_t slot);
    void send_pair_response(const uint8_t* mac, uint16_t sequence, uint16_t slot);
    void send_gw_announce(const uint8_t* mac);
    const uint8_t* dest_for_chip(const uint8_t* mac, uint8_t client_chip);
};

// ---- Compatibility wrapper declarations ----
// Allow callers not yet migrated to the class API to keep compiling.
bool espnow_handler_init();
void espnow_handler_loop();
bool espnow_start_pairing();
void espnow_stop_pairing();
bool espnow_is_pairing();
unsigned long espnow_pairing_remaining_ms();
unsigned long espnow_get_rx_count();
unsigned long espnow_get_ack_count();
unsigned long espnow_get_crc_errors();
uint8_t* espnow_get_gateway_mac();
void espnow_announce();
void espnow_broadcast_time_sync(uint32_t epoch_seconds);
bool espnow_send_command(const uint8_t *mac, uint8_t slot, uint8_t state);
bool espnow_send_restart(const uint8_t *mac, uint8_t slot);

#else

// Stub: minimal no-op EspnowHandler when ESP-NOW is disabled
class EspnowHandler : public RadioInterface {
public:
    int init() override { return -1; }
    int send(const uint8_t*, size_t) override { return -1; }
    void loop() override {}
    bool is_ready() const override { return false; }
    bool start_pairing() override { return false; }
    void stop_pairing() override {}
    bool is_pairing() const override { return false; }
    unsigned long pairing_remaining_ms() const override { return 0; }
    unsigned long get_rx_count() const override { return 0; }
    unsigned long get_ack_count() const override { return 0; }
    unsigned long get_crc_errors() const override { return 0; }
    uint8_t* get_radio_mac() override { return nullptr; }
    void announce() override {}
    void broadcast_time_sync(uint32_t) override {}
    bool send_command(const uint8_t*, uint8_t) override { return false; }
    bool send_restart(const uint8_t*) override { return false; }
};

// Stub wrappers (no-op, use when ESPNOW_ENABLED is not defined)
inline bool espnow_handler_init() { return false; }
inline void espnow_handler_loop() {}
inline bool espnow_start_pairing() { return false; }
inline void espnow_stop_pairing() {}
inline bool espnow_is_pairing() { return false; }
inline unsigned long espnow_pairing_remaining_ms() { return 0; }
inline unsigned long espnow_get_rx_count() { return 0; }
inline unsigned long espnow_get_ack_count() { return 0; }
inline unsigned long espnow_get_crc_errors() { return 0; }
inline uint8_t* espnow_get_gateway_mac() { return nullptr; }
inline void espnow_announce() {}
inline void espnow_broadcast_time_sync(uint32_t) {}
inline bool espnow_send_command(const uint8_t*, uint8_t, uint8_t) { return false; }
inline bool espnow_send_restart(const uint8_t*, uint8_t) { return false; }

#endif

#endif
