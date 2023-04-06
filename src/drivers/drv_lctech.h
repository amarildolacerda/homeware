#pragma once
#include <Arduino.h>
#include <protocol.h>

class LCTechRelayDriver : public Driver
{
private:
    int ultimoStatus = 0;

public:
    static void registerMode()
    {
        registerDriverMode("lc", create);
    }
    static Driver *create()
    {
        return new LCTechRelayDriver();
    }

    int readPin() override
    {
        return ultimoStatus;
    }
    int writePin( const int value) override
    {
        byte relON[] = {0xA0, 0x01, 0x01, 0xA2}; // Hex command to send to serial for open relay
        byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1};
        if (value == 0)
        {
            Serial.write(relOFF, sizeof(relOFF));
        }
        else
            Serial.write(relON, sizeof(relON));
        ultimoStatus = value;
        #ifdef DEBUG_ON
        getProtocol()->debugf("LcTech Relay: %i to: %i",_pin,value);
        #endif
        actionEvent(value);
        return value;
    }

};
