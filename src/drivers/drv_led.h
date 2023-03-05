#pragma once

#include <protocol.h>

#define ledTimeout 200

class LedDriver : public Driver
{
private:
    int ledPin = -1;
    bool ledStatus = false;
    unsigned long ultimoLedChanged = 0;

public:
    void
    setup() override
    {
        Driver::setV1(10000);
        Driver::setMode("led");
        Driver::setup();
    }
    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return ledLoop(pin);
    }
    void loop() override
    {
        ledLoop(ledPin);
    }
    int ledLoop(const int pin = 255)
    {
        ledPin = pin;
        if (pin == 255 || pin < 0)
            ledPin = getProtocol()->findPinByMode("led");
        if (ledPin > -1)
        {
            if (pin < 0)
            {
                ledStatus = false;
                digitalWrite(ledPin, ledStatus);
            }
            else
            {
                long dif = millis() - ultimoLedChanged;
                if (dif > v1)
                {
                    ledStatus = !ledStatus;
                    digitalWrite(ledPin, ledStatus);
                    ultimoLedChanged = millis();
                    //Serial.println("led on");
                }
                else if (ledStatus && millis() - ultimoLedChanged > ledTimeout)
                {
                    ledStatus = false;
                    digitalWrite(ledPin, ledStatus);
                    //Serial.println("led off");
                }
            }
        }
        return ledStatus;
    }
    bool isGet() override { return true; }
    bool isSet() override { return false; }
};
