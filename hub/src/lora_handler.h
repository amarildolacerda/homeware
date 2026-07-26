#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "radio_interface.h"
#include <LoRa.h>

#define LORA_RX_BUF_SIZE 256

class LoraHandler : public RadioInterface {
public:
    LoraHandler() : m_ok(false), m_rx_len(0) {}
    ~LoraHandler() {}

    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

private:
    bool m_ok;
    uint8_t m_rx_buf[LORA_RX_BUF_SIZE];
    int m_rx_len;

    void handle_rx();
};

#endif
