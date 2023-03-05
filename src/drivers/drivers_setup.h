#pragma once

#include "options.h"
//#include <protocol.h>
#include "drivers.h"

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

#define NONE

void drivers_register()
{
    ADCDriver adc = ADCDriver();
    getDrivers()->add(&adc);
#ifndef NONE

    LDRDriver ldr = LDRDriver();
    getDrivers()->add(&ldr);

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
    GrooverUltrasonic drv = GrooverUltrasonic();
    getDrivers()->add(&drv);
#endif
#ifdef DHT_SENSOR
    DHTDriver dht = DHTDriver();
    getDrivers()->add(&dht);
#endif
#endif // NONE
#endif
}