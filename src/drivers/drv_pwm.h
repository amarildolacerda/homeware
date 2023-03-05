#pragma once
#include <Arduino.h>
#include <protocol.h>

class PWMDriver : public Driver
{
private
    :
    unsigned long entrada = millis();

public:
    void setup() override
    {
        Driver::setMode("pwm");
        Driver::setup();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
        active = true;
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return analogRead(pin);
    }
    int writePin(const int pin, const int value) override
    {

        if (value == 0)
        {
            digitalWrite(pin, LOW);
        }
        else
        {
            int timeout = v1;
            analogWrite(pin, value);
            if (value > 0 && timeout > 0)
            {
                const int entrada = millis();
                digitalWrite(pin, HIGH);
                delay(timeout);
                digitalWrite(pin, LOW);
                return millis() - entrada;
            }
        }
        return 0;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
};
