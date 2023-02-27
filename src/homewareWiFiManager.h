#ifndef homewarewifi_h
#define homewarewifi_h

#include "ESP_WifiManager.h"

class HomewareWiFiManager : public WiFiManager
{
public:
    String WiFiManager::pageMake(const String stitle, const String payload)
    {
        String page = getHTTPHead(stitle);  // @token options @todo replace options with title
        String str = FPSTR(HTTP_ROOT_MAIN); // @todo custom title
        str.replace(FPSTR(T_t), stitle);
        str.replace(FPSTR(T_v), configPortalActive ? _apName : (getWiFiHostname() + " - " + WiFi.localIP().toString())); // use ip if ap is not active for heading @todo use hostname?
        page += str;
        page += payload;
        page += FPSTR(HTTP_END);
        return page;
    }

private:
};

#endif