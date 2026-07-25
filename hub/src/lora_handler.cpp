#ifdef HABILITA_LORA

#include "lora_handler.h"
#include "lora_config.h"
#include "lora_protocol.h"

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
    return LoRa.endPacket() ? 0 : -1;
}

bool LoraHandler::is_ready() const {
    return m_ok;
}

void LoraHandler::loop() {
    if (!m_ok) return;
    handle_rx();
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
