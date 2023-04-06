#pragma once
#include <Arduino.h>
#include <protocol.h>

class PWMDriver : public Driver
{
private
    :
    unsigned long entrada = millis();

public:
    static void registerMode()
    {
        registerDriverMode("pwm", create);
    }
    static Driver *create()
    {
        return new PWMDriver();
    }

    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
        active = true;
    }

    int readPin() override
    {
        return analogRead(_pin);
    }
    int writePin( const int value) override
    {

        if (value == 0)
        {
            digitalWrite(_pin, LOW);
        }
        else
        {
            //int timeout = v1;
            analogWrite(_pin, value);
            if (value > 0 && timeout > 0)
            {
                const int entrada = millis();
                digitalWrite(_pin, HIGH);
                delay(timeout);
                digitalWrite(_pin, LOW);
                return millis() - entrada;
            }
        }
        return 0;
    }
};
