#pragma once

#include "api.h"
#include <uMqttBroker.h>
#ifndef BASIC
#include "timer.h"
#endif

class HMqttBroker : public uMQTTBroker
{

public:
    bool onConnect(IPAddress addr, uint16_t client_count) override
    {
        getInstanceOfProtocol()->debugf("Mqtt Client: %s", addr.toString().c_str());
#ifndef BASIC
        publish("broker/on", timer.getNow());
#endif
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
        registerDriver("mqttd", create, true);
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
#ifndef BASIC
            broker->publish("broker/datetime", timer.getNow());
#endif
            lastOne = millis();
        }
    }
};
