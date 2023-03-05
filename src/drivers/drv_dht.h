

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
private:
    int ultimoLeitura = 0;
    JsonObject ultimoStatus;

public:
    void setup() override
    {
        Driver::setMode("dht");
        Driver::setup();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, INPUT);
    }

    bool isCommand() override
    {
        return true;
    }
    String doCommand(const String command) override
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
    JsonObject readStatus(const int pin) override
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
    virtual bool isGet() override { return true; }
    virtual bool isStatus() override { return true; }
    virtual int readPin(const int pin) override
    {
        if (!ultimoStatus || millis() - ultimoLeitura > 60000*5)
        {
            ultimoLeitura = millis();
            Driver::setPin(pin);
            ultimoStatus = readStatus(pin);
        }
        return ultimoStatus["temperature"];
    }
};
