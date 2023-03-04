#pragma once

#include "options.h"

#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
#endif
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
#endif

// expose

#ifdef DHT_SENSOR
JsonObject dhtReadStatus(const int pin)
{
    return DHTDriver().readDht(pin);
}
#endif

void drivers_register()
{
#ifdef GROOVE_ULTRASONIC
    Driver drv = GrooverUltrasonic();
    getDrivers().add(drv);
#endif
#ifdef DHT_SENSOR
    Driver dht = DHTDriver();
    getDrivers().add(dht);
#endif
}