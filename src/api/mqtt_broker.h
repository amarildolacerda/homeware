#pragma once

#include "api.h"
#include <uMqttBroker.h>
#include "timer.h"

class MqttBrokerApi : public ApiDriver
{
private:
    uMQTTBroker *broker;
    unsigned long lastOne = 0;

public:
    static void registerApi()
    {
        registerApiDriver("mqttb", create, true);
    }
    static ApiDriver *create()
    {
        return new MqttBrokerApi();
    }
    MqttBrokerApi(){
        interval = 30000;
    }
    void setup() override
    {
        Serial.print("MQTT Broker: ");
        broker = new uMQTTBroker();

        broker->init();
        Serial.println("OK");
    }

    void loop() override
    {
        if (millis() - lastOne > interval)
        {
            broker->publish("broker/datetime", timer.getNow());
            lastOne = millis();
        }
    }
};
