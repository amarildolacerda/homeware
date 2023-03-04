#pragma once

#include <protocol.h>
#include <Ultrasonic.h>

unsigned long ultimo_ultrasonic = 0;

class GrooverUltrasonic : public Driver
{
    virtual void setup()
    {
        mode = "gus";
        getProtocol()->resources += "gus,";
    };
    virtual void loop(){};
    virtual int read(const int xpin)
    {
        pin = xpin;
        try
        {
            Protocol *protocol = getProtocol();
            if (millis() - ultimo_ultrasonic > (int(protocol->config["interval"]) * 5))
            {
                Ultrasonic ultrasonic(pin);
                long RangeInCentimeters;
                RangeInCentimeters = ultrasonic.MeasureInCentimeters(); // two measurements should keep an interval
                Serial.print(RangeInCentimeters);                       // 0~400cm
                Serial.println(" cm");
                ultimo_ultrasonic = millis();
                return roundf(RangeInCentimeters);
            }
            else
            {
                return int(protocol->docPinValues[String(pin)]);
            }
        }
        catch (const char *e)
        {
            Serial.print("ERRO grooveUltrasonic: ");
            Serial.println(e);
        }
        return -1;
    }
};
