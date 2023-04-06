#pragma once
#include "drivers.h"

class V1Driver : public Driver
{
protected:
private:
    long lastOne = 0;
    float power = 0;
    long v1 = 0;

public:
    static void registerMode()
    {
        registerDriverMode("v1", create);
    }
    static Driver *create()
    {
        return new V1Driver();
    }
    int readPin()
    {
        return (int)v1;
    }
    int writePin(const int value)
    {
        v1 = value;
        actionEvent(value);
        return v1;
    }
};
