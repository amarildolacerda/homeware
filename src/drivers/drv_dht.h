

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
    DHTDriver()
    {
        mode = "dht";
        getProtocol()->resources += "dht,";
    }
    virtual void setup()
    {
        Serial.println("dht enabled");
    }
    virtual String doCommand(const String command)
    {
        Serial.print("command dht: ");
        Serial.println(command);
        String *cmd = split(command, ' ');
        JsonObject j = readStatus(String(cmd[1]).toInt());
        String result;
        serializeJson(j, result);
        getProtocol()->debug(result);
        return result;
    }
    virtual JsonObject readStatus(const int xpin)
    {  
        if (!dht_inited)
        {
            pin = xpin;
            dht.setup(pin, DHTesp::AUTO_DETECT);
            dht_inited = true;
        }
        delay(dht.getMinimumSamplingPeriod());
        docDHT["temperature"] = dht.getTemperature();
        docDHT["humidity"] = dht.getHumidity();
        docDHT["fahrenheit"] = dht.toFahrenheit(dht.getTemperature());
        getProtocol()->debug("Temperatura: " + String(dht.getTemperature()));
        return docDHT.as<JsonObject>();
    }
    virtual bool isGet() { return true; }
    virtual bool isStatus() { return true; }
    virtual int readPin(const int xpin)
    {
        Serial.println("DHT->readPin()");
        pin = xpin;
        return readStatus(pin)["temperature"];
    }
};
