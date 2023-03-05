#pragma once
#include <Arduino.h>
#include <protocol.h>

class OutDriver : public Driver
{
public:
    void setup() override
    {
        Driver::setMode("out");
        Driver::setup();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return digitalRead(pin);
    }
    int writePin(const int pin, const int value) override
    {
        digitalWrite(pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
