#include "display.h"
#include "page_manager.h"
#include "display_interface.h"
#include "config.h"
#include "radio_node_strategy.h"
#include "myWiFiManager.h"
#include <WiFi.h>
#include <Arduino.h>

extern bool s_relay;
extern uint8_t s_my_mac[6];
extern char s_device_id[32];
extern char s_device_name[32];

extern NodeRadioType s_radio;

#ifdef DISPLAY_TTGO
#include "display_ttgo.h"
static Ssd1306DisplayTtgo s_display_obj;
static DisplayInterface* s_display = &s_display_obj;
#elif defined(DISPLAY_HELTEC)
#include "display_heltec.h"
static Ssd1306DisplayHeltec s_display_obj;
static DisplayInterface* s_display = &s_display_obj;
#else
static DisplayInterface* s_display = nullptr;
#endif

static PageManager s_pages(s_display);

static void render_page_0(DisplayInterface &d)
{
    d.set_cursor(0, 0);
    d.print("LoRa Switch");
    d.set_cursor(0, 12);
    d.print(s_relay ? "ON" : "OFF");
    d.set_cursor(0, 24);
    d.print(s_radio.is_paired() ? "Pareado" : "---");
    d.set_cursor(0, 36);
    d.printf("RSSI: %d", s_radio.last_rssi());
    d.set_cursor(0, 48);
    d.print(WiFi.localIP().toString().c_str());
}

static void render_page_1(DisplayInterface &d)
{
    unsigned long sec = millis() / 1000;
    int dd = sec / 86400;
    sec %= 86400;
    int hh = sec / 3600;
    sec %= 3600;
    int mm = sec / 60;
    sec %= 60;
    d.set_cursor(0, 0);
    d.printf("TX: %lu", s_radio.tx_count());
    d.set_cursor(0, 12);
    d.printf("RX: %lu", s_radio.rx_count());
    d.set_cursor(0, 24);
    d.printf("Mem: %u", ESP.getFreeHeap());
    d.set_cursor(0, 36);
    d.printf("Uptime: %dd %02d:%02d:%02d", dd, hh, mm, (int)sec);
    d.set_cursor(0, 48);
    d.printf("WiFi: %s", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "---");
}

void display_init()
{
    if (s_display != nullptr)
    {
        if (!s_display->begin())
            return;
        s_display->set_text_size(1);
        s_display->clear();
        s_display->print("Booting...");
        s_display->display();
        s_pages.add_page(render_page_0);
        s_pages.add_page(render_page_1);
    }
}

void display_loop()
{
    if (s_display != nullptr)
    {
        s_pages.loop();
    }
}
