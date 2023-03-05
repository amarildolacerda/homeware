#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

//=================drivers
#include "drivers/drv_adc.h"
#include "drivers/drv_in.h"
#include "drivers/drv_out.h"

#ifdef DRIVERS_ENABLED
//=================================== init enabled
#include "drivers/drv_ldr.h"
#include "drivers/drv_led.h"
#include "drivers/drv_rst.h"
#include "drivers/drv_pwm.h"

#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
GrooverUltrasonicDriver gus_ = GrooverUltrasonicDriver();
#endif

#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
DHTDriver dht_;
#endif

#include "drivers/drv_lctech.h"
LCTechRelayDriver lcTech_;

LDRDriver ldr_;
LedDriver led_;
ResetButtonDriver rst_;
PWMDriver pwm_;
//=================================fim enabled
#endif

// expose==========================================
// default
ADCDriver adc_;
InDriver in_;
OutDriver out_;

void drivers_register()
{

    Drivers *driv = getDrivers();
// defaults
    driv->add(&adc_);
    driv->add(&in_);
    driv->add(&out_);

// enableds
#ifdef DRIVERS_ENABLED
    driv->add(&ldr_);
    driv->add(&led_);
    driv->add(&pwm_);
    driv->add(&rst_);
    driv->add(&lcTech_);

#ifdef DHT_SENSOR
    driv->add(&dht_);
#endif

#ifdef GROOVE_ULTRASONIC
    driv->add(&gus_);
#endif // GROOVER
#endif // DRIVER_ENABLED
}