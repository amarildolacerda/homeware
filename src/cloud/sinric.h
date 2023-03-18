#pragma once

#include <options.h>
#include <cloud.h>

class SinricCloud : public CloudDriver
{
private:
protected:
public:
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
    static void registerCloud()
    {
        registerCloudDriver("motion", create);
    }
    static CloudDriver *create()
    {
        return new MotionSinricCloud();
    }
    void changed(const int pin, const long value) override;

    void setup() override;
};

void registerSinricApi();
