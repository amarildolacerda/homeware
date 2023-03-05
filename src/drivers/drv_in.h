#pragma once
#include <Arduino.h>
#include <protocol.h>

class InDriver : public Driver
{
public:
    void setup() override
    {
        Driver::setMode("in");
        Driver::setup();
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
