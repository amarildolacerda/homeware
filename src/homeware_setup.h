#include "options.h"
#ifdef PORTAL
#include "portal.h"
#endif
#include "homeware.h"
#ifndef BASIC
#include "timer.h"
#endif

#ifdef ALEXA
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
#ifdef ALEXA
    Alexa::init(&alexa);
#endif
    homeware.setup(&server);
#endif

#ifdef PORTAL
    portal.setup(&server);
    portal.autoConnect(homeware.config["label"]);
#endif

#ifndef BASIC
    timer.update();
    Serial.printf("Ver: %s \r\n", VERSION);
#ifdef PORTAL
    homeware.server->on("/clear", []()
                        {
        Serial.println("reiniciando");
        homeware.server->send(200, "text/html", "reiniciando...");
        portal.reset(); });
#endif
#endif

#ifdef ALEXA
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
#ifdef ALEXA
    Alexa::handle();
#endif

#ifndef BASIC
    timer.update();
#endif    
}
