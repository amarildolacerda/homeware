#ifndef homewarewifi_h
#define homewarewifi_h

#include <WiFiManager.h>

#if !(defined(ESP8266) || defined(ESP32))
#error This code is intended to run on the ESP8266 or ESP32 platform! Please check your Tools->Board setting.
#endif

#if  !( defined(ESP32) || defined(NO_MDNS))
#ifdef WM_MDNS
desativar o WM_MDNS do WiFiManager.
#endif
#endif

    class HomewareWiFiManager : public WiFiManager
{
private:
public:
    String pageMake(const String stitle, const String body)
    {
        String page = getHTTPHead(stitle);  // @token options @todo replace options with title
        String str = FPSTR(HTTP_ROOT_MAIN); // @todo custom title
        str.replace(FPSTR(T_t), stitle);
        str.replace(FPSTR(T_v), getConfigPortalActive()
                                    ? getConfigPortalSSID()
                                    : (getWiFiHostname() + " - " + WiFi.localIP().toString())); // use ip if ap is not active for heading @todo use hostname?
        page += str;
        page += body;
        page += FPSTR(HTTP_END);
        return page;
    }
};

#endif