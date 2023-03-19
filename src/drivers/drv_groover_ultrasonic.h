#pragma once

#include <protocol.h>
#include <Ultrasonic.h>

unsigned long ultimo_ultrasonic = 0;

class GrooverUltrasonicDriver : public Driver
{
private:
    int lastValue;
    int param = 100;
    unsigned interval = 1000;
    unsigned eventState = 0;
    bool trgOkState = true;

public:
    static void registerMode()
    {
        registerDriverMode("gus", create);
    }
    static Driver *create()
    {
        return new GrooverUltrasonicDriver();
    }

    void setup() override
    {
        Driver::setup();
        if (getProtocol()->containsKey("gus"))
            param = getProtocol()->getKey("gus").toInt();
        if (getProtocol()->containsKey("gus_interval"))
            interval = getProtocol()->getKey("gus_interval").toInt();
        triggerEnabled = true;
    }
    void setPinMode(int pin) override
    {
        Driver::setPinMode(pin);
        pinMode(pin, OUTPUT);
        active = true;
    }

    int readPin() override
    {
        try
        {
            // Protocol *protocol = getProtocol();
            if (millis() - ultimo_ultrasonic > interval)
            {
                Ultrasonic ultrasonic(_pin);
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
        if (getStatus() != eventState)
        {
            eventState = getStatus();
            triggerCallback(getMode(), getPin(), eventState);
            trgOkState = true;
        }
        if (trgOkState)
        {
            triggerOkState("ok", getPin(), eventState == 0 ? HIGH : LOW);
            trgOkState = false;
        }
    }
    bool isLoop() override { return active; }
};
