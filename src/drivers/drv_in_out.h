#pragma once
#include <Arduino.h>
#include <protocol.h>

class InOutDriver : public Driver
{
public:
    virtual void setup() override
    {
        Driver::setup();
    }
    int readPin(const int pin) override
    {
        active = true;

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

class InDriver : public InOutDriver
{
public:
    InDriver()
    {
        setMode("in");
    }
    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, INPUT);
    }
};

class DownDriver : public InOutDriver
{
public:
    DownDriver()
    {
        setMode("pulldown");
    }
    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
#ifdef ARDUINO_AVR
        pinMode(pin, INPUT);
#else
        pinMode(pin, INPUT_PULLDOWN_16);
#endif
    }
};
class UpDriver : public InOutDriver
{
public:
    UpDriver()
    {
        setMode("pullup");
    }
    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, INPUT_PULLUP);
    }
};
class OutDriver : public InOutDriver
{
public:
    OutDriver()
    {
        setMode("out");
    }
    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, OUTPUT);
    }
};
