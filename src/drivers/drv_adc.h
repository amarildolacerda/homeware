#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
public:
    void setup() override
    {
        Driver::setMode("adc");
        Driver::setup();
    }
    void setPinMode(int pin) override
    {
        active = true;
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return analogRead(pin);
    }
    int writePin(const int pin, const int value) override
    {
        analogWrite(pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
