#ifdef LORA_ENABLED

#include "lora_handler.h"
#include "lora_config.h"
#include <string.h>

// ── RadioHead header prepended to all outgoing packets ──
// AVR nodes use RH_RF95 which expects: [TO][FROM][ID][FLAGS] before payload
#define RH_HEADER_SIZE 4
static const uint8_t s_rh_header[RH_HEADER_SIZE] = {0xFF, 0x00, 0x01, 0x00}; // TO=broadcast, FROM=hub, ID=1, FLAGS=0

LoraHandler::LoraHandler()
    : m_radio([]{
        LoraSpiConfig cfg;
        cfg.ss = LORA_SS;
        cfg.rst = LORA_RST;
        cfg.dio0 = LORA_DIO0;
        cfg.sck = LORA_SCK;
        cfg.miso = LORA_MISO;
        cfg.mosi = LORA_MOSI;
        cfg.freq = LORA_FREQ;
        cfg.sf = LORA_SF;
        cfg.bw = LORA_BW * 1E3f;
        cfg.cr = LORA_CR;
        cfg.preamble = LORA_PREAMBLE;
        cfg.tx_power = LORA_TX_POWER;
        return cfg;
    }())
{}

int LoraHandler::init() { return m_radio.init(); }
void LoraHandler::loop() { m_radio.loop(); }
bool LoraHandler::is_ready() const { return m_radio.is_ready(); }

// Send with RadioHead header prepended (for AVR nodes compatibility)
int LoraHandler::send(const uint8_t* data, size_t len) {
    uint8_t buf[RH_HEADER_SIZE + LORA_MAX_PAYLOAD];
    if (len > LORA_MAX_PAYLOAD) return -1;
    memcpy(buf, s_rh_header, RH_HEADER_SIZE);
    memcpy(buf + RH_HEADER_SIZE, data, len);
    return m_radio.send(buf, RH_HEADER_SIZE + len);
}

bool LoraHandler::send_command(const uint8_t* mac, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

bool LoraHandler::send_restart(const uint8_t* mac) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = 0xFF;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

#endif
