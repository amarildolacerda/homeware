#pragma once

#include <Arduino.h>
#include <SoftwareSerial.h>

// ── SX1276 Registers (LoRa mode) ──
#define SX_REG_FIFO          0x00
#define SX_REG_OP_MODE       0x01
#define SX_REG_FRF_MSB       0x06
#define SX_REG_FRF_MID       0x07
#define SX_REG_FRF_LSB       0x08
#define SX_REG_PA_CONFIG     0x09
#define SX_REG_OCP           0x0B
#define SX_REG_LNA           0x0C
#define SX_REG_FIFO_ADDR_PTR      0x0D
#define SX_REG_FIFO_TX_BASE_ADDR  0x0E
#define SX_REG_FIFO_RX_BASE_ADDR  0x0F
#define SX_REG_FIFO_RX_CUR_ADDR   0x10
#define SX_REG_IRQ_FLAGS          0x12
#define SX_REG_RX_NB_BYTES        0x13
#define SX_REG_PKT_RSSI_VALUE     0x1A
#define SX_REG_PAYLOAD_LENGTH    0x22
#define SX_REG_MODEM_CONFIG_1     0x1D
#define SX_REG_MODEM_CONFIG_2     0x1E
#define SX_REG_MODEM_CONFIG_3     0x26
#define SX_REG_SYNC_WORD          0x39
#define SX_REG_DIO_MAPPING_1      0x40
#define SX_REG_VERSION            0x42
#define SX_REG_PA_DAC             0x4D

// LoRa mode
#define SX_LORA_MODE    0x80
#define SX_SLEEP_MODE   0x00
#define SX_STANDBY_MODE 0x01
#define SX_TX_MODE      0x03
#define SX_RX_CONT_MODE 0x05

// IRQ flags
#define SX_IRQ_RX_DONE  0x40
#define SX_IRQ_TX_DONE  0x08

// ── Bridge Protocol (ATMega168 Grove firmware) ──
// TX: 'W' + reg + len + [data...]  → write SPI register
// RX: 'R' + reg + len              → read SPI register (returns data)

// ── Driver ──
class LoraUartDriver {
public:
    LoraUartDriver(SoftwareSerial &serial, uint8_t ss_pin = 10);

    bool init(float freq_mhz, uint8_t sf, uint32_t bw_hz, uint8_t cr, uint8_t tx_power);

    bool send(const uint8_t *data, uint8_t len);
    bool recv(uint8_t *buf, uint8_t *len, int8_t *rssi);

    bool is_ready() const { return m_ready; }
    int16_t last_rssi() const { return m_last_rssi; }

private:
    void sx_write_reg(uint8_t reg, uint8_t val);
    uint8_t sx_read_reg(uint8_t reg);
    void sx_write_fifo(const uint8_t *data, uint8_t len);
    uint8_t sx_read_fifo(uint8_t *data, uint8_t max_len);
    void sx_set_mode(uint8_t mode);

    SoftwareSerial &m_serial;
    uint8_t m_ss_pin;
    bool m_ready;
    int16_t m_last_rssi;
};
