#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "radio_interface.h"
#include "lora_spi_radio.h"
#include "lora_protocol.h"
#include "sensor_registry.h"
#include "config.h"

class LoraHandler : public RadioInterface {
public:
    LoraHandler();
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;
    void set_rx_callback(rx_callback_t cb, void* arg = nullptr) {
        m_radio.set_rx_callback(cb, arg);
    }
private:
    void check_offline_sensors();
    LoraSpiRadio m_radio;
};

#endif
