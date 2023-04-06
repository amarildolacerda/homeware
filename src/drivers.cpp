
#include "drivers.h"

Drivers *drivers = new Drivers();

Drivers *getDrivers()
{
    return drivers;
}

ModeDriverPair modeDriverList[DRIVERS_SIZE];
int numModes = 0;
void registerDriverMode(String mode, Driver *(*create)())
{
    modeDriverList[numModes] = {mode, create};
    numModes++;
#ifndef ARDUINO_AVR
    getInstanceOfProtocol()->resources += mode + ",";
#endif
}

Driver *createByDriverMode(const String mode, const int pin)
{
    for (ModeDriverPair drvMode : modeDriverList)
        if (drvMode.mode == mode)
        {
            Driver *drv = drvMode.create();
            drv->_mode = mode;
            drv->setPinMode(pin);
            drv->beforeSetup();
#ifdef DEBUG_ON
            getInstanceOfProtocol()->debugf("DRV: %s at %i\r\n", drv->_mode, drv->_pin);
#endif
            return drv;
        }
    return nullptr;
}

Driver *Drivers::initPinMode(const String mode, const int pin)
{
    Driver *old = findByPin(pin);
    if (old && old->_mode == mode)
        return old;
    Driver *nwer = createByDriverMode(mode, pin);
    if (nwer)
    {
        items[driversCount++] = nwer;
        return nwer;
    }
    return nullptr;
}

void Drivers::add(Driver *driver)
{
    items[driversCount++] = driver;
}
