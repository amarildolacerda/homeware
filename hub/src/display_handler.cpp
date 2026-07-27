#ifdef HABILITA_DISPLAY_TTGO

#include "display_handler.h"
#include "display_config.h"
#include "config.h"
#include "sensor_registry.h"
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "platform.h"
#include "common_console.h"

static Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, DISPLAY_RST);
static bool s_display_ok = false;
static unsigned long s_last_page_switch = 0;
static int s_current_page = 0;
static const int s_page_count = 3;

static int s_last_rssi = 0;
static char s_last_device_name[32] = "";

static char s_chip_id_str[16];
static char s_platform_str[16];

void display_handler_init(void) {
#ifdef HELTEC_W32LA
    pinMode(DISPLAY_RST, OUTPUT);
    digitalWrite(DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(DISPLAY_RST, HIGH);
    delay(10);
#endif
    s_display_ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR);
    console.printf("[Display] begin() at 0x%02X: %s\n", DISPLAY_ADDR, s_display_ok ? "OK" : "FAIL");

    if (!s_display_ok) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Booting...");
    display.display();

    snprintf(s_chip_id_str, sizeof(s_chip_id_str), "0x%06x", chip_id());
    snprintf(s_platform_str, sizeof(s_platform_str), "%s", PLATFORM_PREFIX);

    s_last_page_switch = millis();
}

static void find_newest_device(void) {
    int newest_slot = -1;
    unsigned long newest_time = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->last_seen > newest_time) {
            newest_time = s->last_seen;
            newest_slot = i;
        }
    }
    if (newest_slot >= 0) {
        virtual_sensor_t *s = sensor_registry_get(newest_slot);
        if (s) {
            s_last_rssi = s->last_rssi;
            strncpy(s_last_device_name, s->name, sizeof(s_last_device_name) - 1);
            s_last_device_name[sizeof(s_last_device_name) - 1] = '\0';
        }
    }
}

static void render_page_0(void) {
    display.setCursor(0, 0);
    display.println(WiFi.localIP());
    display.println();
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    display.printf("%d/%d online\n", online, paired);
    unsigned long sec = millis() / 1000;
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    display.printf("%02d:%02d:%02d\n", h, m, s);
}

static void render_page_1(void) {
    display.setCursor(0, 0);
    display.println(FW_VERSION);
    display.printf("ID: %s\n", s_chip_id_str);
    display.printf("Plat: %s\n", s_platform_str);
}

static void render_page_2(void) {
    display.setCursor(0, 0);
    display.printf("RSSI: %d dBm\n", s_last_rssi);
    if (strlen(s_last_device_name) > 0) {
        display.printf("Dev: %s\n", s_last_device_name);
    } else {
        display.println("Dev: --");
    }
    int t = temperatureRead();
    if (t < 200) {
        display.printf("Temp: %d C\n", t);
    }
}

void display_handler_loop(void) {
    if (!s_display_ok) return;

    unsigned long now = millis();
    if (now - s_last_page_switch < DISPLAY_PAGE_MS) return;
    s_last_page_switch = now;

    s_current_page = (s_current_page + 1) % s_page_count;
    find_newest_device();

    display.clearDisplay();
    switch (s_current_page) {
        case 0: render_page_0(); break;
        case 1: render_page_1(); break;
        case 2: render_page_2(); break;
    }
    display.display();
}

#endif
