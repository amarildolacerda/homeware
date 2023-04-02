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
        registerApiDriver("mqttb", create);
    }
    static ApiDriver *create()
    {
        return new MqttBrokerApi();
    }

    void setup() override
    {
        broker = new uMQTTBroker();
        broker->init();
    }

    void loop() override
    {
        if (millis() - lastOne > 60000)
        {
            broker->publish("broker/datetime", timer.getNow());
        }
    }
};
