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

    void setPinMode(int pin) override
    {
        pinMode(pin, INPUT);
        active = true;
    }

    int readPin() override
    {
        int v = digitalRead(_pin);
        if (v == HIGH)
        {
            getProtocol()->reset();
        }
        return v;
    }
    int writePin( const int value) override
    {
        debug(value);
        digitalWrite(_pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
