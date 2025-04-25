#include "options.h"
#ifdef PORTAL
#ifndef WIFI // defined(WIFI_ENABLED)
erro, Portal requer WIFI habilitado
#endif
#include "portal.h"
#endif
#include "homeware.h"

#ifdef TIMER
#include "timer.h"
#endif

#if defined(ALEXA)
#ifndef WIFI // defined(WIFI_ENABLED)
          erro,
    Alexa requer WIFI habilitado
#endif
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
#if defined(ALEXA)
    Alexa::init(&alexa);
#endif
    homeware.setup(&server);
#endif

#ifdef PORTAL
    portal.setup(&server);
    portal.autoConnect(homeware.config["label"]);
#endif

#ifdef TIMER
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

#if defined(ALEXA)
    alexa.begin(&server);
#endif
}

//----------------------------------------------------------------------------------
void homeware_loop()
//----------------------------------------------------------------------------------
{
#ifdef PORTAL
    portal.loop(); // checa reconecta;
#endif
#ifdef ARDUINO_AVR
    prot.loop();
#else
    homeware.loop();
#endif
#if defined(ALEXA)
    Alexa::handle();
#endif

#ifdef TIMER
    timer.update();
#endif
    DEBUGF("homeware_loop() end\r\n");
}
