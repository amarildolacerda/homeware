#include "options.h"
#ifndef NO_WIFI
#include "portal.h"
#endif
#include "homeware.h"

#ifndef NO_TIMER
#include "timer.h"
#endif

#if defined(ALEXA) && defined(WIFI_ENABLED)
#include <Espalexa.h>
#include "api/alexa.h"
Espalexa alexa = Espalexa();
#endif

#ifdef ARDUINO_AVR
#include "protocol.h"
Protocol prot;
#endif

//----------------------------------------------------------------------------------
void homeware_setup()
//----------------------------------------------------------------------------------
{
#ifdef ARDUINO_AVR
    prot.setup();
#else
    Serial.printf("\r\n\r\n");
    homeware.prepare();
#if defined(ALEXA) && defined(WIFI_ENABLED)
    Alexa::init(&alexa);
#endif
    homeware.setup(&server);
#endif

#ifndef NO_WIFI
    portal.setup(&server);
    portal.autoConnect(homeware.config["label"]);
#endif

#ifndef BASIC
#ifndef NO_TIMER
    timer.update();
#endif
    Serial.printf("Ver: %s \r\n", VERSION);
#ifdef PORTAL
    homeware.server->on("/clear", []()
                        {
        Serial.println("reiniciando");
        homeware.server->send(200, "text/html", "reiniciando...");
        portal.reset(); });
#endif
#endif

#if defined(ALEXA) && defined(WIFI_ENABLED)
    alexa.begin(&server);
#endif


}

//----------------------------------------------------------------------------------
void homeware_loop()
//----------------------------------------------------------------------------------
{
#ifndef NO_WIFI
    portal.loop(); // checa reconecta;
#endif
#ifdef ARDUINO_AVR
    prot.loop();
#else
    homeware.loop();
#endif
#if defined(ALEXA) && defined(WIFI_ENABLED)
    Alexa::handle();
#endif

#ifndef NO_TIMER
    timer.update();
#endif    
}
