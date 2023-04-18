#pragma once

#include "api.h"
#include "functions.h"

class MqttClientDriver : public ApiDriver
{
protected:
    int port = 1883;
    String host = "none";
    String user;
    String password;
    String prefix;
    String name;
    long lastOne = 0;
    unsigned long lastAlive = 0;
    unsigned long interval = 60000;

    void init();

public:
    String clientId;
    static void
    registerApi()
    {
        registerApiDriver("mqtt", create, true);
    }
    static ApiDriver *create()
    {
        return new MqttClientDriver();
    }
    MqttClientDriver()
    {
        clientId = String(getChipId(), HEX);
    }
    bool isConnected(bool force=false);
    void setup() override;
    void loop() override;
    bool send(const char *subtopic, const char *payload);
    bool sendScene(const char *scene, const int value);
    bool isEnabled();
    void sendAlive();
    void subscribes();
    void changed(const String value)override;
    void reload()override;
};

MqttClientDriver *getMqtt();