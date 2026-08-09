#include "lora_uart_driver.h"

LoraUartDriver::LoraUartDriver(SoftwareSerial &serial, uint8_t ss_pin)
    : m_serial(serial), m_ss_pin(ss_pin), m_ready(false), m_last_rssi(0) {}

// ── SPI via ATMega168 bridge ──
// ATMega168 firmware accepts: 'W' + reg + len + [data] (write)
//                             'R' + reg + len          (read, returns data)

void LoraUartDriver::sx_write_reg(uint8_t reg, uint8_t val) {
    m_serial.write('W');
    m_serial.write(reg);
    m_serial.write((uint8_t)1);
    m_serial.write(val);
    delay(2); // wait for SPI transfer
}

uint8_t LoraUartDriver::sx_read_reg(uint8_t reg) {
    m_serial.write('R');
    m_serial.write(reg);
    m_serial.write((uint8_t)1);
    delay(2);
    while (!m_serial.available());
    return m_serial.read();
}

void LoraUartDriver::sx_write_fifo(const uint8_t *data, uint8_t len) {
    sx_write_reg(SX_REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(SX_REG_FIFO_TX_BASE_ADDR, 0x00);
    sx_write_reg(SX_REG_PAYLOAD_LENGTH, len);

    m_serial.write('W');
    m_serial.write((uint8_t)SX_REG_FIFO);
    m_serial.write(len);
    for (uint8_t i = 0; i < len; i++) m_serial.write(data[i]);
    delay(2);
}

uint8_t LoraUartDriver::sx_read_fifo(uint8_t *data, uint8_t max_len) {
    uint8_t len = sx_read_reg(SX_REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    sx_write_reg(SX_REG_FIFO_RX_CUR_ADDR, sx_read_reg(SX_REG_FIFO_RX_CUR_ADDR));

    m_serial.write('R');
    m_serial.write((uint8_t)SX_REG_FIFO);
    m_serial.write(len);
    delay(2);
    for (uint8_t i = 0; i < len; i++) {
        while (!m_serial.available());
        data[i] = m_serial.read();
    }
    return len;
}

void LoraUartDriver::sx_set_mode(uint8_t mode) {
    sx_write_reg(SX_REG_OP_MODE, SX_LORA_MODE | mode);
}

// ── Init ──

bool LoraUartDriver::init(float freq_mhz, uint8_t sf, uint32_t bw_hz, uint8_t cr, uint8_t tx_power) {
    m_serial.begin(9600);
    delay(200);

    // Flush any pending data
    while (m_serial.available()) m_serial.read();

    // Verify SX1276 version
    uint8_t ver = sx_read_reg(SX_REG_VERSION);
    if (ver != 0x12) return false;

    // Sleep + LoRa mode
    sx_set_mode(SX_SLEEP_MODE);
    delay(10);

    // Frequency
    uint32_t frf = (uint32_t)((double)(freq_mhz * 1E6) * 524288.0 / 32000000.0);
    sx_write_reg(SX_REG_FRF_MSB, (frf >> 16) & 0xFF);
    sx_write_reg(SX_REG_FRF_MID, (frf >> 8) & 0xFF);
    sx_write_reg(SX_REG_FRF_LSB, frf & 0xFF);

    // Modem config 1: BW + CR + explicit header off
    uint8_t bw_val = 0;
    if (bw_hz <= 7800) bw_val = 0;
    else if (bw_hz <= 10400) bw_val = 0x10;
    else if (bw_hz <= 15600) bw_val = 0x20;
    else if (bw_hz <= 20800) bw_val = 0x30;
    else if (bw_hz <= 31250) bw_val = 0x40;
    else if (bw_hz <= 41700) bw_val = 0x50;
    else if (bw_hz <= 62500) bw_val = 0x60;
    else if (bw_hz <= 125000) bw_val = 0x70;
    else bw_val = 0x70; // default 125kHz

    uint8_t cr_val = ((cr - 4) & 0x07) << 1;
    sx_write_reg(SX_REG_MODEM_CONFIG_1, bw_val | cr_val | 0x00); // explicit header off

    // Modem config 2: SF
    sx_write_reg(SX_REG_MODEM_CONFIG_2, (sf & 0x0F) << 4);

    // Modem config 3: AGC auto on
    sx_write_reg(SX_REG_MODEM_CONFIG_3, 0x04);

    // Preamble 8 symbols
    sx_write_reg(0x20, 0x00); // PREAMBLE_MSB
    sx_write_reg(0x21, 0x08); // PREAMBLE_LSB

    // Sync word 0x12 (same as hub)
    sx_write_reg(SX_REG_SYNC_WORD, 0x12);

    // PA config: PA_BOOST, max power
    sx_write_reg(SX_REG_PA_CONFIG, 0x8F);
    sx_write_reg(SX_REG_OCP, 0x2B);
    sx_write_reg(SX_REG_PA_DAC, 0x87);

    // LNA max gain
    sx_write_reg(SX_REG_LNA, 0x23);

    // DIO0 mapping: 0x00 = RxDone/TxDone
    sx_write_reg(SX_REG_DIO_MAPPING_1, 0x00);

    // FIFO addresses
    sx_write_reg(SX_REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(SX_REG_FIFO_TX_BASE_ADDR, 0x00);
    sx_write_reg(SX_REG_FIFO_RX_BASE_ADDR, 0x00);

    // Clear IRQ flags
    sx_write_reg(SX_REG_IRQ_FLAGS, 0xFF);

    // Standby
    sx_set_mode(SX_STANDBY_MODE);

    m_ready = true;
    return true;
}

// ── TX ──

bool LoraUartDriver::send(const uint8_t *data, uint8_t len) {
    if (!m_ready) return false;

    sx_set_mode(SX_STANDBY_MODE);
    delay(1);
    sx_write_fifo(data, len);
    sx_write_reg(SX_REG_IRQ_FLAGS, 0xFF);
    sx_set_mode(SX_TX_MODE);

    // Poll for DIO0 (TxDone) — via bridge, check IRQ register
    unsigned long start = millis();
    while (millis() - start < 3000) {
        uint8_t irq = sx_read_reg(SX_REG_IRQ_FLAGS);
        if (irq & SX_IRQ_TX_DONE) {
            sx_write_reg(SX_REG_IRQ_FLAGS, 0xFF);
            sx_set_mode(SX_STANDBY_MODE);
            return true;
        }
        delay(10);
    }
    sx_set_mode(SX_STANDBY_MODE);
    return false;
}

// ── RX ──

bool LoraUartDriver::recv(uint8_t *buf, uint8_t *len, int8_t *rssi) {
    if (!m_ready) return false;

    uint8_t irq = sx_read_reg(SX_REG_IRQ_FLAGS);
    if (!(irq & SX_IRQ_RX_DONE)) return false;

    sx_write_reg(SX_REG_IRQ_FLAGS, 0xFF);
    *len = sx_read_fifo(buf, 255);
    *rssi = sx_read_reg(SX_REG_PKT_RSSI_VALUE) - 157;
    m_last_rssi = *rssi;

    // Re-enter RX mode
    sx_write_reg(SX_REG_FIFO_ADDR_PTR, 0x00);
    sx_set_mode(SX_RX_CONT_MODE);

    return true;
}
