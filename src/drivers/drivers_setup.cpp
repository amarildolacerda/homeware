#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

#include "drivers/drv_adc.h"
#include "drivers/drv_ldr.h"
#include "drivers/drv_led.h"
#include "drivers/drv_in.h"
#include "drivers/drv_rst.h"

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
#endif
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
DHTDriver dht_;
#endif
#endif

// expose

#define NONE

ADCDriver adc_;
LDRDriver ldr_;
LedDriver led_;
InDriver in_;
ResetButtonDriver rst_;

void drivers_register()
{
    Drivers *driv = getDrivers();
    driv->add(&adc_);
    driv->add(&ldr_);
    driv->add(&led_);
    driv->add(&in_);
    driv->add(&rst_);

#ifdef DHT_SENSOR
    driv->add(&dht_);
#endif

#ifndef NONE

#ifdef DRIVERS_ENABLED
#ifdef GROOVE_ULTRASONIC
    GrooverUltrasonic drv = GrooverUltrasonic();
    getDrivers()->add(&drv);
#endif
#endif // NONE
#endif
}