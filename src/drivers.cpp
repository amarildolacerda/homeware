
#include "drivers.h"

// Drivers *drivers = Drivers::getInstance();
Drivers *drivers = new Drivers();

Drivers *getDrivers()
{
    return drivers;
}
