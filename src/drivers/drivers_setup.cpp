#include "drivers/drivers_setup.h"

// #include <protocol.h>
#include "drivers.h"

//=================drivers
#ifndef NO_DRV_ADC
#include "drivers/drv_adc.h"
#endif
#include "drivers/drv_in_out.h"
#ifndef BASIC
#include "drivers/drv_v1.h"
#endif

#ifdef LCTECHRELAY
#include "drivers/drv_lctech.h"
#endif

#if defined(DRIVERS_EX) || defined(SONOFF_BASIC)
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


#ifdef BUZZER
#include "drivers/drv_buzzer.h"
#endif
#ifdef LEDBAR
#include "drivers/drv_ledbar.h"
#endif

#ifndef BASIC
#if defined(ESP8266) || defined(ESP32)
#include <drivers/drv_vcc.h>
#endif
#endif

bool drivers_loaded = false;
void drivers_register()
{
    if (drivers_loaded)
        return;

        //  defaults
#ifndef NO_DRV_ADC
    ADCDriver::registerMode();
#endif
    InDriver::registerMode();
    OutDriver::registerMode();

#if defined(DRIVERS_EX)
    LedDriver::registerMode();
    OkDriver::registerMode();
    DownDriver::registerMode();
    UpDriver::registerMode();
    SireneDriver::registerMode();
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
#else
#ifdef SONOFF_BASIC
    LedDriver::registerMode();
    SireneDriver::registerMode();
#endif
#endif

#ifdef LCTECHRELAY
    LCTechRelayDriver::registerMode();
#endif
#ifdef BUZZER
    BuzzerDriver::registerMode();
#endif
#ifdef LEDBAR
    LedBarDriver::registerMode();
#endif

#ifndef BASIC
#if defined(ESP8266) || defined(ESP32)
    VccDriver::registerMode();
#endif
#endif

#ifndef BASIC
    V1Driver::registerMode();
#endif
    drivers_loaded = true;
}