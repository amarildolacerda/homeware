#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

//=================drivers
#include "drivers/drv_adc.h"
#include "drivers/drv_in_out.h"

#if defined(DRIVERS_EX)
#include "drivers/drv_led.h"
#include "drivers/drv_srn.h"
#ifdef LCTECHRELAY
#include "drivers/drv_lctech.h"
LCTechRelayDriver lcTech_;
#endif
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
DHTDriver dht_;
#endif
#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
GrooverUltrasonicDriver gus_ = GrooverUltrasonicDriver();
#endif
#endif

#ifdef DRIVERS_ENABLED
//=================================== init enabled
#include "drivers/drv_ldr.h"
#include "drivers/drv_rst.h"
#include "drivers/drv_pwm.h"

LDRDriver ldr_;
ResetButtonDriver rst_;
PWMDriver pwm_;
//=================================fim enabled
#endif

// expose==========================================
// default
ADCDriver adc_;
InDriver in_;
OutDriver out_;
#if defined(DRIVERS_EX)
LedDriver led_;
DownDriver down_;
UpDriver up_;
SireneDriver srn_;
PulseDriver pls_;
#endif

void drivers_register()
{

    Drivers *driv = getDrivers();
    // defaults
    driv->add(&adc_);
    driv->add(&in_);
    driv->add(&out_);
#if defined(DRIVERS_EX)
    driv->add(&led_);
    driv->add(&down_);
    driv->add(&up_);
    driv->add(&srn_);
    driv->add(&pls_);
#ifdef LCTECHRELAY
    driv->add(&lcTech_);
#endif
#ifdef DHT_SENSOR
    driv->add(&dht_);
#endif

#ifdef GROOVE_ULTRASONIC
    driv->add(&gus_);
#endif // GROOVER

// enableds
#ifdef DRIVERS_ENABLED
    driv->add(&ldr_);
    driv->add(&pwm_);
    driv->add(&rst_);
#endif // DRIVER_ENABLED
#endif
}