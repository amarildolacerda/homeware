#pragma once

#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

class TimerClass
{
private:
    WiFiUDP ntpUDP;
    NTPClient timeClient = NTPClient(ntpUDP, "pool.ntp.org");

public:
    TimerClass()
    {
        timeClient.begin();
       
        
        // timeClient.setTimeOffset(-3600*3);
    }

    void update()
    {
        timeClient.update();
    }

    String getNow()
    {
        return formatDateTime(timeClient.getEpochTime());
    }

    String formatDateTime(time_t epochTime)
    {
        char buffer[20];
        struct tm *timeinfo;
        timeinfo = localtime(&epochTime);
        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
        return String(buffer);
    }
};

extern TimerClass timer ;
