
#include "drivers.h"

// Drivers *drivers = Drivers::getInstance();
Drivers *drivers = new Drivers();

Drivers *getDrivers()
{
    return drivers;
}

ModeDriverPair modeDriverList[32];
int numModes = 0;
void registerDriverMode(String mode, Driver *(*create)())
{
    modeDriverList[numModes] = {mode, create};
    numModes++;
    drivers->getProtocol()->resources += mode + ",";
}

Driver *createByDriverMode(const String mode, const int pin)
{
    for (ModeDriverPair drvMode : modeDriverList)
        if (drvMode.mode == mode)
        {
            Driver *drv = drvMode.create();
            return drv;
        }
    return nullptr;
}
