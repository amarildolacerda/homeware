
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
#ifndef NO_WM
    HomewareWiFiManager wifiManager = HomewareWiFiManager();
#endif
    ESP8266WebServer *server;
    void setup(ESP8266WebServer *externalServer = nullptr);
#endif
    String label;
    void autoConnect(const String label);
    void reset();
    void loop();

private:
#ifndef NO_WM
    void setupServer();
#endif
};

extern Portal portal;

#endif