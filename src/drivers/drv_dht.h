

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
        Driver::setMode ( "dht");
        getProtocol()->resources += "dht,";
        Serial.println("Init DHTDriver");
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
    virtual JsonObject readStatus(const int pin)
    {  
        if (!dht_inited)
        {
            Driver::setPin(pin);
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
    virtual int readPin(const int pin)
    {
        Serial.println("DHT->readPin()");
        Driver::setPin(pin);
        return readStatus(pin)["temperature"];
    }
};
