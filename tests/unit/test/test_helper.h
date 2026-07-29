#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>   // memset, memcpy

// Stub millis() para testes
static unsigned long s_fake_millis = 0;
unsigned long millis() { return s_fake_millis; }
void fake_millis_set(unsigned long t) { s_fake_millis = t; }
void fake_millis_advance(unsigned long delta) { s_fake_millis += delta; }

// RadioInterface (mesmo caminho que shared/ usa)
#include "radio_interface.h"

// MockRadio: implementa RadioInterface, grava sends p/ inspeção
class MockRadio : public RadioInterface {
public:
    bool m_ready = true;
    int m_init_ret = 0;
    uint8_t m_last_sent[256];
    size_t m_last_sent_len = 0;
    int m_send_ret = 0;
    bool m_init_called = false;

    int init() override { m_init_called = true; return m_init_ret; }
    int send(const uint8_t* data, size_t len) override {
        m_last_sent_len = len < 256 ? len : 256;
        memcpy(m_last_sent, data, m_last_sent_len);
        return m_send_ret;
    }
    void loop() override {}
    bool is_ready() const override { return m_ready; }

    // Helper para injetar pacotes RX como se o rádio tivesse recebido
    void inject_rx(const uint8_t* data, size_t len, int16_t rssi) {
        if (m_rx_cb) m_rx_cb(data, len, rssi, m_rx_arg);
    }
};
