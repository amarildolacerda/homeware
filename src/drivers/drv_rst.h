#pragma once

#include <protocol.h>

class ResetButtonDriver : public Driver
{
public:
    static void registerMode()
    {
        registerDriverMode("rst", create);
    }
    static Driver *create()
    {
        return new ResetButtonDriver();
    }

    void setup() override
    {
        Driver::setMode("rst");
        Driver::setup();
    }
    void setPinMode(int pin) override
    {
        Driver::setPin(pin);
        pinMode(pin, INPUT);
        active = true;
    }

    int readPin(const int pin) override
    {
        int v = digitalRead(pin);
        if (v == HIGH)
        {
            getProtocol()->reset();
        }
        return v;
    }
    int writePin(const int pin, const int value) override
    {
        digitalWrite(pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
