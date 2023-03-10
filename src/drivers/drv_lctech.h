#pragma once
#include <Arduino.h>
#include <protocol.h>

class LCTechRelayDriver : public Driver
{
private:
    int ultimoStatus = 0;

public:
    void setup() override
    {
        Driver::setMode("lc");
        Driver::setup();
    }
    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return ultimoStatus;
    }
    int writePin(const int pin, const int value) override
    {
        byte relON[] = {0xA0, 0x01, 0x01, 0xA2}; // Hex command to send to serial for open relay
        byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1};
        if (value == 0)
        {
            Serial.write(relOFF, sizeof(relOFF));
        }
        else
            Serial.write(relON, sizeof(relON));
        ultimoStatus = value;
        return value;
    }

    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
