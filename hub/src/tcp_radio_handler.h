#ifndef TCP_RADIO_HANDLER_H
#define TCP_RADIO_HANDLER_H

#include "radio_interface.h"
#include "tcp_protocol.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUDP.h>
#include <map>
#include <string>
#include <vector>

#ifdef TCP_ENABLED

struct PendingCommand {
    String command;
    uint8_t slot;
    unsigned long created_at;
};

class TcpRadioHandler : public RadioInterface {
public:
    TcpRadioHandler();
    ~TcpRadioHandler() {}

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

    uint8_t* get_radio_mac() override { return nullptr; }
    void announce() override;
    void broadcast_time_sync(uint32_t epoch_seconds) override;

    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;

    void handle_register(const uint8_t* mac, const char* device_id, uint8_t sensor_type, const char* device_name, const char* fw_version);
    void handle_state(const char* device_id, JsonObject& state);
    void handle_heartbeat(const char* device_id);
    bool handle_command_get(const char* device_id, JsonObject& response);

public:
    static TcpRadioHandler* s_self;

private:
    bool m_pairing_mode = false;
    unsigned long m_pairing_start = 0;
    unsigned long m_rx_count = 0;
    unsigned long m_ack_count = 0;
    unsigned long m_crc_errors = 0;

    WiFiUDP m_udp;
    std::map<std::string, std::vector<PendingCommand>> m_pending_commands;

    void handle_udp_discover();
    void cleanup_expired_commands();
    int find_slot_by_device_id(const char* device_id);
};

// Compatibility wrappers
bool tcp_handler_init();
void tcp_handler_loop();
bool tcp_send_command(const uint8_t* mac, uint8_t slot, uint8_t state);
bool tcp_send_restart(const uint8_t* mac, uint8_t slot);

#else

class TcpRadioHandler : public RadioInterface {
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

inline bool tcp_handler_init() { return false; }
inline void tcp_handler_loop() {}
inline bool tcp_send_command(const uint8_t*, uint8_t, uint8_t) { return false; }
inline bool tcp_send_restart(const uint8_t*, uint8_t) { return false; }

#endif

#endif