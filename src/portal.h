
#ifndef portal_def
#define portal_def

#include "options.h"

#include "homeware.h"

class Portal
{
public:
#ifdef ESP32
    HomewareWiFiManager wifiManager;
    WebServer *server;
    void setup(WebServer *externalServer = nullptr);
#else
    HomewareWiFiManager wifiManager = HomewareWiFiManager();
    ESP8266WebServer *server;
    void setup(ESP8266WebServer *externalServer = nullptr);
#endif
    String label;
    void autoConnect(const String label);
    void reset();
    void loop();

private:
    void setupServer();
};

extern Portal portal;

#endif