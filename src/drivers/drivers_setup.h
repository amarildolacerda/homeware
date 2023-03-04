#pragma once

#include "options.h"
#include <protocol.h>

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
#endif
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
#endif
#endif

#include <drivers/drv_adc.h>
#include <drivers/drv_ldr.h>
// expose

void drivers_register()
{
    getDrivers().add(ADCDriver());
    getDrivers().add(LDRDriver());

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
    Driver drv = GrooverUltrasonic();
    getDrivers().add(drv);
#endif
#ifdef DHT_SENSOR
    Driver dht = DHTDriver();
    getDrivers().add(dht);
#endif
#endif
}