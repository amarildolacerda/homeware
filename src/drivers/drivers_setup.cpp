#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

//=================drivers
#include "drivers/drv_adc.h"
#include "drivers/drv_in_out.h"

#ifdef LCTECHRELAY
#include "drivers/drv_lctech.h"
// LCTechRelayDriver lcTech_;
#endif

#if defined(DRIVERS_EX)
#include "drivers/drv_led.h"
#include "drivers/drv_srn.h"
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
// DHTDriver dht_;
#endif
#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
//GrooverUltrasonicDriver gus_ = GrooverUltrasonicDriver();
#endif
#endif

#ifdef DRIVERS_ENABLED
//=================================== init enabled
#include "drivers/drv_ldr.h"
#include "drivers/drv_rst.h"
#include "drivers/drv_pwm.h"

//LDRDriver ldr_;
//ResetButtonDriver rst_;
//PWMDriver pwm_;
//=================================fim enabled
#endif

// expose==========================================
// default
// ADCDriver adc_;
// InDriver in_;
// OutDriver out_;
#if defined(DRIVERS_EX)
// LedDriver led_;
//DownDriver down_;
//UpDriver up_;
//SireneDriver srn_;
//PulseDriver pls_;
#endif

bool drivers_loaded = false;
void drivers_register()
{
    if (drivers_loaded)
        return;

    //Drivers *driv = getDrivers();
    // defaults
    ADCDriver::registerMode();

    // driv->add(&adc_);
    InDriver::registerMode();
    // driv->add(&in_);
    OutDriver::registerMode();
    // driv->add(&out_);

#ifdef LCTECHRELAY
    //    driv->add(&lcTech_);
    LCTechRelayDriver::registerMode();

#endif

#if defined(DRIVERS_EX)
    LedDriver::registerMode();
    // driv->add(&led_);
    // driv->add(&down_);
    DownDriver::registerMode();
    // driv->add(&up_);
    UpDriver::registerMode();
    // driv->add(&srn_);
    SireneDriver::registerMode();
    // driv->add(&pls_);
    PulseDriver::registerMode();
#ifdef DHT_SENSOR
    DHTDriver::registerMode();
    // driv->add(&dht_);
#endif

#ifdef GROOVE_ULTRASONIC
    // driv->add(&gus_);
    GrooverUltrasonicDriver::registerMode();
#endif // GROOVER

// enableds
#ifdef DRIVERS_ENABLED
    // driv->add(&ldr_);
    LDRDriver::registerMode();
    // driv->add(&pwm_);
    PWMDriver::registerMode();
    // driv->add(&rst_);
    ResetButtonDriver::registerMode();
#endif // DRIVER_ENABLED
#endif
    drivers_loaded = true;
}