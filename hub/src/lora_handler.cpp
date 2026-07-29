#ifdef HABILITA_LORA

#include "lora_handler.h"
#include "lora_config.h"
#include "lora_protocol.h"
#include <string.h>

int LoraHandler::init() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ * 1E6)) {
        return -1;
    }
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW * 1E3);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setPreambleLength(LORA_PREAMBLE);
    LoRa.receive();
    m_ok = true;
    return 0;
}

int LoraHandler::send(const uint8_t* data, size_t len) {
    if (!m_ok) return -1;
    LoRa.beginPacket();
    LoRa.write(data, len);
    int ret = LoRa.endPacket() ? 0 : -1;
    LoRa.receive();
    return ret;
}

bool LoraHandler::is_ready() const {
    return m_ok;
}

void LoraHandler::loop() {
    if (!m_ok) return;
    handle_rx();
}

bool LoraHandler::send_command(const uint8_t* mac, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

bool LoraHandler::send_restart(const uint8_t* mac) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = 0xFF; // restart
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

void LoraHandler::handle_rx() {
    int len = LoRa.parsePacket();
    if (len <= 0 || len > LORA_RX_BUF_SIZE) return;

    int i = 0;
    while (LoRa.available() && i < LORA_RX_BUF_SIZE) {
        m_rx_buf[i++] = LoRa.read();
    }
    m_rx_len = i;

    int16_t rssi = LoRa.packetRssi();

    if (m_rx_len >= LORA_HEADER_SIZE && m_rx_cb) {
        m_rx_cb(m_rx_buf, m_rx_len, rssi, m_rx_arg);
    }
}

#endif // HABILITA_LORA
