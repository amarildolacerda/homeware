
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
            drv->setPin(pin);
            drv->setMode(mode);
            drv->setPinMode(pin);
            drv->beforeSetup();
            getInstanceOfProtocol()->debugf("DRV: %s at %i\r\n", drv->getMode(), drv->getPin());
            return drv;
        }
    return nullptr;
}

Driver *Drivers::initPinMode(const String mode, const int pin)
{
    Driver *old = findByPin(pin);
    if (old && old->getMode() == mode)
        return old;
    Driver *nwer = createByDriverMode(mode, pin);
    if (nwer)
    {
        deleteByPin(pin);
        items[driversCount++] = nwer;
#ifdef DEBUG_DRV
        // MAP;
        String s = "";
        int i = 0;
        for (Driver *d : items)
        {
            if (d)
            {
                if (s != "")
                    s += ",";
                s += "'" + String(i++) + "':";
                s += "'" + d->getMode() + "'";
            }
        }
        s = "{" + s + "}";
        Serial.println(s);
        Serial.printf("Criou %s em %i, count: %i\r\n", mode.c_str(), pin, driversCount);
#endif
        return nwer;
    }
    return nullptr;
}

// template <class T>
void Drivers::add(Driver *driver)
{
    items[driversCount++] = driver;
}
