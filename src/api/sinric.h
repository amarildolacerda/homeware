#pragma once
#ifdef SINRICPRO

#include <options.h>
#include <api.h>

class SinricCloud : public ApiDriver
{
private:
protected:
public:
    int sensorIndex = -1;
    String sensorId;
    static void initSinric();
    void loop() override;
    void afterSetup() override
    {
        SinricCloud::initSinric();
    }
    virtual void setup() override;
    static int findSinricPin(String id);
    static bool onSinricPowerState(const String &deviceId, bool &state);
};

class MotionSinricCloud : public SinricCloud
{
public:
    static void registerApi()
    {
        registerDriver("motion", create);
        registerDriver("alarm", create);
    }
    static ApiDriver *create()
    {
        return new MotionSinricCloud();
    }
    void changed(const int pin, const long value) override;

    void setup() override;
};

class DoorbellSinricCloud : public SinricCloud
{
public:
    void setup() override;
    void changed(const int pin, const long value) override;
    static void registerApi()
    {
        registerDriver("doorbell", create);
    }
    static ApiDriver *create()
    {
        return new DoorbellSinricCloud();
    }
};

class TemperatureSinricCloud : public SinricCloud
{
protected:
public:
    void setup() override;
    void changed(const int pin, const long value) override;
    static void registerApi()
    {
        registerDriver("dht", create);
    }
    static ApiDriver *create()
    {
        return new TemperatureSinricCloud();
    }
    static bool onSinricDHTPowerState(const String &deviceId, bool &state);
};

void registerSinricApi();

#endif