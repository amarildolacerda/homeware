#pragma once
#include "drivers.h"

class V1Driver : public Driver
{
protected:
private:
    long lastOne = 0;
    float power = 0;

public:
    static void
    registerMode()
    {
        registerDriverMode("v1", create);
    }
    static Driver *create()
    {
        return new V1Driver();
    }
    void setPinMode(int pin) override
    {
        // pinMode(pin, OUTPUT);
    }
    bool isSet() override { return true; }
    bool isLoop() override { return false; }
    int readPin()
    {
        return (int)v1;
    }
    int writePin(const int value)
    {
        v1 = value;
        debug(value);
        return v1;
    }
};
