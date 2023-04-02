#pragma once

#include "api.h"
#include <uMqttBroker.h>
#include "timer.h"

class HMqttBroker : public uMQTTBroker
{

public:
    bool onConnect(IPAddress addr, uint16_t client_count) override
    {
        getInstanceOfProtocol()->debugf("Mqtt Client: %s", addr.toString().c_str());
        publish("broker/on", timer.getNow());
        return true;
    }
};
class MqttBrokerApi : public ApiDriver
{
private:
    HMqttBroker *broker;
    unsigned long lastOne = 0;
    unsigned long interval = 30000;

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
        broker = new HMqttBroker();

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
