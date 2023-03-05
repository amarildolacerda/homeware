#pragma once

#include <protocol.h>
#include <Ultrasonic.h>

unsigned long ultimo_ultrasonic = 0;

class GrooverUltrasonicDriver : public Driver
{
private:
    int lastValue;
    int param = 100;
    unsigned ultimoEvento = 0;
    unsigned interval = 30000;
    unsigned ultimoStatus = 0;

public:
    void setup() override
    {
        Driver::setMode("gus");
        Driver::setup();
        if (getProtocol()->containsKey("gus"))
            param = getProtocol()->getKey("gus").toInt();
        if (getProtocol()->containsKey("gus_interval"))
            interval = getProtocol()->getKey("gus_interval").toInt();
        Serial.printf("gus param: %i", param);
    }
    void setPinMode(int pin) override
    {
        Driver::setPinMode(pin);
        pinMode(pin, OUTPUT);
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        try
        {
            Protocol *protocol = getProtocol();
            if (millis() - ultimo_ultrasonic > interval)
            {
                Ultrasonic ultrasonic(pin);
                long RangeInCentimeters;
                RangeInCentimeters = ultrasonic.MeasureInCentimeters(); // two measurements should keep an interval
                // Serial.print(RangeInCentimeters);                       // 0~400cm
                getProtocol()->debug("leitura ultrasonic " + String(RangeInCentimeters) + " cm");
                // Serial.println(" cm");
                ultimo_ultrasonic = millis();
                lastValue = roundf(RangeInCentimeters);
                // Serial.print(lastValue);
                return lastValue;
            }
            else
            {
                return lastValue;
            }
        }
        catch (const char *e)
        {
            Serial.print("ERRO grooveUltrasonic: ");
            Serial.println(e);
        }
        return -1;
    }
    unsigned int getStatus()
    {
        return (lastValue < param ? 1 : 0);
    }
    void loop() override
    {
        if (millis() - ultimoEvento > interval)
        {
            if (getStatus() != ultimoStatus)
            {
                ultimoStatus = getStatus();
                triggerCallback(getMode(), getPin(), ultimoStatus);
                ultimoEvento = millis();
            }
        }
    }
    bool isLoop() override { return true; }
};
