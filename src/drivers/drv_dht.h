

#pragma once

#include <Arduino.h>
#include <protocol.h>
#include <ArduinoJson.h>
#include <functions.h>

#include "DHTesp.h"
DHTesp dht;
bool dht_inited = false;
StaticJsonDocument<200> docDHT;

/*
#ifdef DHT_SENSOR
            else if (cmd[2] == "get" && getMode()[cmd[1]] == "dht")
            {
                JsonObject j = readDht(String(cmd[1]).toInt());
                String result;
                serializeJson(j, result);
                return result;
            }
#endif
*/
class DHTDriver : public Driver
{
public:
    virtual void setup()
    {
        mode = "dht";
        getProtocol()->resources += "dht,";
    }
    virtual String doCommand(const String command){
        String *cmd = split(command, ' ');
        JsonObject j = readDht(String(cmd[1]).toInt());
        String result;
        serializeJson(j, result);
        return result;
    }
    JsonObject readDht(const int pin)
    {
        if (!dht_inited)
        {
            dht.setup(pin, DHTesp::AUTO_DETECT);
            dht_inited = true;
        }
        delay(dht.getMinimumSamplingPeriod());
        //homeware.ledLoop(255); //(-1) desliga
        docDHT["temperature"] = dht.getTemperature();
        docDHT["humidity"] = dht.getHumidity();
        docDHT["fahrenheit"] = dht.toFahrenheit(dht.getTemperature());
        Serial.print("Temperatura: ");
        Serial.println(dht.getTemperature());
        return docDHT.as<JsonObject>();
    }

    virtual int readPin(const int xpin)
    {
        pin = xpin;
        return readDht(pin)["temperature"];
    }
};
