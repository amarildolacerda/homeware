
#ifndef portal_def
#define portal_def

#include <options.h>

#ifdef WIFI_NEW
#include <WManager.h>
#else
#include <WiFiManager.h>
#endif

class Portal
{
public:
    char *hostname;
    WiFiManager wifiManager = WiFiManager();
    ESP8266WebServer *server;
    String label;
    void setup(ESP8266WebServer *externalServer = nullptr);
    void autoConnect(const String label);
    void reset();
    void loop();

private:
    void setupServer();
};

extern Portal portal;

#endif