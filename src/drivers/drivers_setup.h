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
    ADCDriver* adc =  new ADCDriver();
    getDrivers().add(adc);

    LDRDriver* ldr = new LDRDriver();
    getDrivers().add(ldr);

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
    GrooverUltrasonic* drv = new GrooverUltrasonic();
    getDrivers().add(drv);
#endif
#ifdef DHT_SENSOR
    DHTDriver* dht = new DHTDriver();
    getDrivers().add(dht);
#endif
#endif
}