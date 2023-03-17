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
    static void registerMode()
    {
        registerDriverMode("in", create);
    }
    static Driver *create()
    {
        return new InDriver();
    }

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
    static void registerMode()
    {
        registerDriverMode("pulldown", create);
    }
    static Driver *create()
    {
        return new DownDriver();
    }
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
#ifdef ESP32
        pinMode(pin, INPUT_PULLDOWN);
#else
        pinMode(pin, INPUT_PULLDOWN_16);
#endif
#endif
    }
};
class UpDriver : public InOutDriver
{
public:
    static void registerMode()
    {
        registerDriverMode("pullup", create);
    }
    static Driver *create()
    {
        return new UpDriver();
    }

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
    static void registerMode()
    {
        registerDriverMode("out", create);
    }
    static Driver *create()
    {
        return new OutDriver();
    }

    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, OUTPUT);
    }
};
