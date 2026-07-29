#pragma once
#include <stdint.h>
#include <stddef.h>

class LoRaClass {
public:
    void setPins(int8_t ss, int8_t rst, int8_t dio0) {}
    bool begin(long freq) { return true; }
    void setSpreadingFactor(int sf) {}
    void setSignalBandwidth(long bw) {}
    void setCodingRate4(int cr) {}
    void setTxPower(int txPower) {}
    void setPreambleLength(long len) {}
    void receive() {}
    int beginPacket() { return 1; }
    size_t write(const uint8_t* buf, size_t len) { return len; }
    int endPacket(bool async = false) { return 1; }
    int parsePacket(int size = 0) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int packetRssi() { return 0; }
};
extern LoRaClass LoRa;

class SPIClass {
public:
    void begin(int sck, int miso, int mosi, int ss) {}
};
extern SPIClass SPI;
