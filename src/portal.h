
#ifndef portal_def
#define portal_def

#include <options.h>

#ifdef WIFI_NEW
#include <WManager.h>
#else
#include <ESP_WiFiManager.h>
#endif

class Portal
{
public:

#ifdef ESP32
    ESP_WiFiManager wifiManager ;
    WebServer *server;
    void setup(WebServer *externalServer = nullptr);
#else
    WiFiManager wifiManager = WiFiManager();
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