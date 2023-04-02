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

    void setup() override
    {
        Serial.print("MQTT Broker: ");
        broker = new uMQTTBroker();

        broker->init();
        Serial.println("OK");
    }

    void loop() override
    {
        if (millis() - lastOne > 60000)
        {
            broker->publish("broker/datetime", timer.getNow());
        }
    }
};
