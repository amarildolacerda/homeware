#pragma once

#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

class TelegramBot
{
private:
    String BOT_TOKEN;
    UniversalTelegramBot bot;
    WiFiClientSecure secured_client;

public:
    void setup()
    {
        secured_client = WiFiClientSecure();
        bot = UniversalTelegramBot(BOT_TOKEN, secured_client);
    }
    void loop()
    {
    }
};