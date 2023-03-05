#pragma once

#include <protocol.h>

#define ledTimeout 200

class LedDriver : public Driver
{
private:
    int ledPin = -1;
    bool ledStatus = false;
    size_t ultimoLedChanged = 0;

public:
    void
    setup() override
    {
        Driver::setMode("led");
        getProtocol()->resources += "led,";
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
                if (millis() - ultimoLedChanged > v1)
                {
                    ledStatus = !ledStatus;
                    digitalWrite(ledPin, ledStatus);
                    ultimoLedChanged = millis();
                }
                else if (ledStatus && millis() - ultimoLedChanged > ledTimeout)
                {
                    ledStatus = false;
                    digitalWrite(ledPin, ledStatus);
                }
            }
        }
        return ledStatus;
    }
    bool isGet() override { return true; }
    bool isSet() override { return false; }
};
