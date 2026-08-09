// nodes/onoff-lora-avr/firmware/lora_bridge/lora_bridge.ino
// Firmware for ATMega168 on Seeed Grove LoRa module
// Bridges UART (from Arduino Nano) to SX1276 SPI in raw mode.
// No external libraries — direct SPI register access.

#include <SPI.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ── SX1276 Registers (LoRa mode) ──
#define REG_FIFO          0x00
#define REG_OP_MODE       0x01
#define REG_FRF_MSB       0x06
#define REG_FRF_MID       0x07
#define REG_FRF_LSB       0x08
#define REG_PA_CONFIG     0x09
#define REG_OCP           0x0B
#define REG_LNA           0x0C
#define REG_FIFO_ADDR_PTR      0x0D
#define REG_FIFO_TX_BASE_ADDR  0x0E
#define REG_FIFO_RX_BASE_ADDR  0x0F
#define REG_FIFO_RX_CUR_ADDR   0x10
#define REG_IRQ_FLAGS_MASK     0x11
#define REG_IRQ_FLAGS          0x12
#define REG_RX_NB_BYTES        0x13
#define REG_PKT_SNR_VALUE      0x19
#define REG_PKT_RSSI_VALUE     0x1A
#define REG_MODEM_CONFIG_1     0x1D
#define REG_MODEM_CONFIG_2     0x1E
#define REG_SYMB_TIMEOUT_LSB   0x1F
#define REG_PREAMBLE_MSB       0x20
#define REG_PREAMBLE_LSB       0x21
#define REG_PAYLOAD_LENGTH     0x22
#define REG_MAX_PAYLOAD_LENGTH 0x23
#define REG_MODEM_CONFIG_3     0x26
#define REG_SYNC_WORD          0x39
#define REG_INVERTIQ           0x33
#define REG_IMAGE_CAL          0x3B
#define REG_TEMP               0x3C
#define REG_DIO_MAPPING_1      0x40
#define REG_VERSION            0x42
#define REG_PA_DAC             0x4D

// LoRa mode bits
#define LORA_MODE         0x80
#define SLEEP_MODE        0x00
#define STANDBY_MODE      0x01
#define FSTX_MODE         0x02
#define TX_MODE           0x03
#define FSRX_MODE         0x04
#define RX_CONTINUOUS_MODE 0x05
#define RX_SINGLE_MODE    0x06

// IRQ flags
#define IRQ_RX_DONE       0x40
#define IRQ_TX_DONE       0x08
#define IRQ_CAD_DONE      0x04
#define IRQ_CRC_ERROR     0x20

// ── Pin Definitions (Seeed Grove LoRa v1.0) ──
#define SX1276_CS   10  // PB2
#define SX1276_DIO0 2   // PD2 (INT0)
#define SX1276_DIO1 3   // PD3 (INT1)
#define SX1276_RST  A1  // PC1

// ── LoRa Configuration (must match hub) ──
#define LORA_FREQ_HZ    868000000UL
#define LORA_SF         10
#define LORA_BW         125000UL
#define LORA_CR         7       // 4/7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17
#define LORA_SYNC_WORD  0x12

// ── UART ──
#define UART_BAUD 9600

// ── State ──
static volatile bool s_dio0_fired = false;

// ── SX1276 SPI Helpers ──
static void sx_write_reg(uint8_t reg, uint8_t val) {
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(reg | 0x80);
    SPI.transfer(val);
    digitalWrite(SX1276_CS, HIGH);
}

static uint8_t sx_read_reg(uint8_t reg) {
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(reg & 0x7F);
    uint8_t val = SPI.transfer(0x00);
    digitalWrite(SX1276_CS, HIGH);
    return val;
}

static void sx_write_fifo(const uint8_t *data, uint8_t len) {
    sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    sx_write_reg(REG_PAYLOAD_LENGTH, len);
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) SPI.transfer(data[i]);
    digitalWrite(SX1276_CS, HIGH);
}

static uint8_t sx_read_fifo(uint8_t *data, uint8_t max_len) {
    uint8_t len = sx_read_reg(REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    sx_write_reg(REG_FIFO_RX_CUR_ADDR, sx_read_reg(REG_FIFO_RX_CUR_ADDR));
    digitalWrite(SX1276_CS, LOW);
    SPI.transfer(REG_FIFO & 0x7F);
    for (uint8_t i = 0; i < len; i++) data[i] = SPI.transfer(0x00);
    digitalWrite(SX1276_CS, HIGH);
    return len;
}

// ── SX1276 Configuration ──
static void sx_set_mode(uint8_t mode) {
    sx_write_reg(REG_OP_MODE, LORA_MODE | mode);
}

static void sx_configure() {
    // Reset
    pinMode(SX1276_RST, OUTPUT);
    digitalWrite(SX1276_RST, LOW);
    delay(10);
    digitalWrite(SX1276_RST, HIGH);
    delay(10);

    // Verify version
    uint8_t ver = sx_read_reg(REG_VERSION);
    if (ver != 0x12) {
        while (1); // Halt on error — wrong chip or not connected
    }

    // Set LoRa mode + sleep
    sx_set_mode(SLEEP_MODE);
    delay(5);

    // Frequency: 868 MHz
    // FRF = freq * 2^19 / 32MHz
    uint32_t frf = (uint32_t)((double)LORA_FREQ_HZ * 524288.0 / 32000000.0);
    sx_write_reg(REG_FRF_MSB, (frf >> 16) & 0xFF);
    sx_write_reg(REG_FRF_MID, (frf >> 8) & 0xFF);
    sx_write_reg(REG_FRF_LSB, frf & 0xFF);

    // Modem config 1: BW=125kHz, CR=4/7, explicit header off
    sx_write_reg(REG_MODEM_CONFIG_1, 0x76); // 0111 0110

    // Modem config 2: SF=10, RX payload CRC off (we handle in app)
    sx_write_reg(REG_MODEM_CONFIG_2, (LORA_SF << 4) | 0x00);

    // Modem config 3: low data rate optimize off, AGC auto on
    sx_write_reg(REG_MODEM_CONFIG_3, 0x04);

    // Preamble length
    sx_write_reg(REG_PREAMBLE_MSB, 0x00);
    sx_write_reg(REG_PREAMBLE_LSB, LORA_PREAMBLE);

    // Sync word
    sx_write_reg(REG_SYNC_WORD, LORA_SYNC_WORD);

    // PA config: PA_BOOST, max power
    sx_write_reg(REG_PA_CONFIG, 0x8F); // PA_BOOST, max power

    // OCP
    sx_write_reg(REG_OCP, 0x2B); // 100mA

    // PA DAC for +20dBm
    sx_write_reg(REG_PA_DAC, 0x87);

    // LNA: max gain
    sx_write_reg(REG_LNA, 0x23);

    // DIO0 = RxDone
    sx_write_reg(REG_DIO_MAPPING_1, 0x00);

    // FIFO base addresses
    sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    sx_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
    sx_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);

    // Clear IRQ flags
    sx_write_reg(REG_IRQ_FLAGS, 0xFF);

    // Standby
    sx_set_mode(STANDBY_MODE);
}

// ── DIO0 Interrupt ──
void dio0_isr() {
    s_dio0_fired = true;
}

// ── UART Helpers ──
static void uart_send_byte(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = b;
}

static void uart_send_buf(const uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) uart_send_byte(buf[i]);
}

static int uart_available() {
    return (UCSR0A & (1 << RXC0)) ? 1 : 0;
}

static uint8_t uart_read() {
    return UDR0;
}

// ── TX Packet ──
static bool sx_tx_packet(const uint8_t *data, uint8_t len) {
    sx_set_mode(STANDBY_MODE);
    delay(1);
    sx_write_fifo(data, len);
    sx_write_reg(REG_IRQ_FLAGS, 0xFF);
    sx_set_mode(TX_MODE);
    unsigned long start = millis();
    while (!s_dio0_fired) {
        if (millis() - start > 3000) return false;
    }
    s_dio0_fired = false;
    sx_set_mode(STANDBY_MODE);
    return true;
}

// ── RX Packet (non-blocking check) ──
static bool sx_rx_check(uint8_t *data, uint8_t *len, int8_t *rssi) {
    if (!s_dio0_fired) return false;
    s_dio0_fired = false;

    uint8_t irq = sx_read_reg(REG_IRQ_FLAGS);
    sx_write_reg(REG_IRQ_FLAGS, 0xFF);

    if (irq & IRQ_RX_DONE) {
        *len = sx_read_fifo(data, 255);
        *rssi = sx_read_reg(REG_PKT_RSSI_VALUE) - 157;
        sx_set_mode(STANDBY_MODE);
        return true;
    }
    return false;
}

// ── Setup ──
void setup() {
    Serial.begin(UART_BAUD);

    pinMode(SX1276_CS, OUTPUT);
    digitalWrite(SX1276_CS, HIGH);
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV4);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    pinMode(SX1276_DIO0, INPUT);
    attachInterrupt(digitalPinToInterrupt(SX1276_DIO0), dio0_isr, RISING);

    sx_configure();
}

// ── Loop ──
void loop() {
    if (uart_available()) {
        uint8_t cmd = uart_read();

        switch (cmd) {
            case 'T': {
                while (!uart_available());
                uint8_t len_hi = uart_read();
                while (!uart_available());
                uint8_t len_lo = uart_read();
                uint16_t len = ((uint16_t)len_hi << 8) | len_lo;
                if (len > 255) len = 255;

                uint8_t buf[255];
                for (uint16_t i = 0; i < len; i++) {
                    while (!uart_available());
                    buf[i] = uart_read();
                }

                bool ok = sx_tx_packet(buf, len);
                uart_send_byte(ok ? 'T' : 'E');
                break;
            }

            case 'R': {
                sx_set_mode(STANDBY_MODE);
                sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
                sx_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
                sx_write_reg(REG_IRQ_FLAGS, 0xFF);
                sx_set_mode(RX_CONTINUOUS_MODE);
                uart_send_byte('T');
                break;
            }

            case '?': {
                uart_send_byte('T');
                break;
            }
        }
    }

    uint8_t buf[255];
    uint8_t len = 0;
    int8_t rssi = 0;

    if (sx_rx_check(buf, &len, &rssi)) {
        uart_send_byte('D');
        uart_send_byte((len >> 8) & 0xFF);
        uart_send_byte(len & 0xFF);
        uart_send_buf(buf, len);

        sx_write_reg(REG_FIFO_ADDR_PTR, 0x00);
        sx_write_reg(REG_IRQ_FLAGS, 0xFF);
        sx_set_mode(RX_CONTINUOUS_MODE);
    }
}
