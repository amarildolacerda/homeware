#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

//=================drivers
#include "drivers/drv_adc.h"
#include "drivers/drv_in_out.h"

#ifdef LCTECHRELAY
#include "drivers/drv_lctech.h"
#endif

#if defined(DRIVERS_EX)
#include "drivers/drv_led.h"
#include "drivers/drv_srn.h"
#ifdef DHT_SENSOR
#include <drivers/drv_dht.h>
#endif
#ifdef GROOVE_ULTRASONIC
#include <drivers/drv_groover_ultrasonic.h>
#endif
#endif

#ifdef DRIVERS_ENABLED
//=================================== init enabled
#include "drivers/drv_ldr.h"
#include "drivers/drv_rst.h"
#include "drivers/drv_pwm.h"
//=================================fim enabled
#endif

bool drivers_loaded = false;
void drivers_register()
{
    if (drivers_loaded)
        return;

    //  defaults
    ADCDriver::registerMode();
    InDriver::registerMode();
    OutDriver::registerMode();

#if defined(DRIVERS_EX)
    LedDriver::registerMode();
    OkDriver::registerMode();
    DownDriver::registerMode();
    UpDriver::registerMode();
    SireneDriver::registerMode();
    PulseDriver::registerMode();
#ifdef DHT_SENSOR
    DHTDriver::registerMode();
#endif


#ifdef GROOVE_ULTRASONIC
    GrooverUltrasonicDriver::registerMode();
#endif // GROOVER

// enableds
#ifdef DRIVERS_ENABLED
    LDRDriver::registerMode();
    PWMDriver::registerMode();
    ResetButtonDriver::registerMode();
#endif // DRIVER_ENABLED
#endif

#ifdef LCTECHRELAY
    LCTechRelayDriver::registerMode();
#endif

    drivers_loaded = true;
}