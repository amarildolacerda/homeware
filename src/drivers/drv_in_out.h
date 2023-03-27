#pragma once
#include <Arduino.h>
#include <protocol.h>

class InOutDriver : public Driver
{
public:
    int readPin() override
    {
        active = true;
        return debug(digitalRead(_pin));
    }
    int writePin(const int value) override
    {
        digitalWrite(_pin, value);
        debug(value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};

class InDriver : public InOutDriver
{
private:
    int oldValue = 0;

public:
    static void registerMode()
    {
        registerDriverMode("in", create);
    }
    static Driver *create()
    {
        return new InDriver();
    }

    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, INPUT);
    }
};

class DownDriver : public InDriver
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
class UpDriver : public InDriver
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

    void setPinMode(int pin) override
    {
        InOutDriver::setPinMode(pin);
        pinMode(pin, INPUT_PULLUP);
    }
};
class OutDriver : public InOutDriver
{
private:
    long lastOne = 0;

public:
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
    bool isLoop() override
    {
        return timeout > 0;
    }
    void loop() override
    {
        if (timeout > 0 && millis() - lastOne > timeout && readPin() > 0)
        {
#ifdef DEBUG_ON
            Serial.println("desligando...");
#endif
            InOutDriver::writePin(0);
            lastOne = millis();
        }
    }
    int writePin(const int value) override
    {
        if (timeout > 0 && value == 0)
            return readPin(); // controlado pelo timer;
        updateTimeout(_pin);
        lastOne = millis();
        return InOutDriver::writePin(value);
    }
};
