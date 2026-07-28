#ifdef HABILITA_DISPLAY

#include "display_handler.h"
#include "display_config.h"
#include "config.h"
#include "sensor_registry.h"
#include <Wire.h>
#include "platform.h"
#include "common_console.h"
#include "mqtt_client.h"
#include "espnow_handler.h"
#include "common_util.h"
#include <time.h>

#ifdef HELTEC_W32LA
#include <HT_SSD1306Wire.h>
static SSD1306Wire display(DISPLAY_ADDR, 400000, DISPLAY_SDA, DISPLAY_SCL, GEOMETRY_128_64, DISPLAY_RST);
#define DISP_WHITE WHITE
#define DISP_BLACK BLACK
#define fill_rect(x,y,w,h,c) do { display.setColor(c); display.fillRect(x,y,x+(w),y+(h)); } while(0)
#define set_text_color(fg,bg) display.setColor(fg)
#define set_text_size(n) do { switch(n){case 1:display.setFont(ArialMT_Plain_10);break;case 2:display.setFont(ArialMT_Plain_16);break;default:display.setFont(ArialMT_Plain_10);} } while(0)
static int s_cx = 0, s_cy = 0;
#define cursor(x,y) do { s_cx=x; s_cy=y; } while(0)
#define disp_print(msg) do { display.drawString(s_cx,s_cy,msg); s_cx+=display.getStringWidth(msg); } while(0)
#define disp_println(msg) do { disp_print(msg); s_cx=0; s_cy+=10; } while(0)
#define disp_printf(fmt,...) do { char b[64]; snprintf(b,sizeof(b),fmt,##__VA_ARGS__); disp_print(String(b)); } while(0)
#define disp_clear() display.clear()
#define disp_display() display.display()
#else
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
static Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, DISPLAY_RST);
#define DISP_WHITE SSD1306_WHITE
#define DISP_BLACK SSD1306_BLACK
#define fill_rect(x,y,w,h,c) display.fillRect(x,y,w,h,c)
#define set_text_color(fg,bg) display.setTextColor(fg,bg)
#define set_text_size(n) display.setTextSize(n)
#define cursor(x,y) display.setCursor(x,y)
#define disp_print(msg) display.print(msg)
#define disp_println(msg) display.println(msg)
#define disp_printf(fmt,...) display.printf(fmt,##__VA_ARGS__)
#define disp_clear() display.clearDisplay()
#define disp_display() display.display()
#endif

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
    delay(50);
    digitalWrite(DISPLAY_RST, HIGH);
    delay(50);
    s_display_ok = display.init();
    if (s_display_ok) {
        display.displayOn();
        display.flipScreenVertically();
    }
#else
    s_display_ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR);
#endif
    console.printf("[Display] begin() at 0x%02X: %s\n", DISPLAY_ADDR, s_display_ok ? "OK" : "FAIL");

    if (!s_display_ok) return;

    disp_clear();
    set_text_size(1);
    set_text_color(DISP_WHITE, DISP_BLACK);
    cursor(0, 0);
    disp_print("Booting...");
    disp_display();

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

#define FOOTER_Y    52
#define FOOTER_H    12

static void render_footer(void) {
    fill_rect(0, FOOTER_Y, DISPLAY_WIDTH, FOOTER_H, DISP_WHITE);
    set_text_color(DISP_BLACK, DISP_WHITE);
    cursor(0, FOOTER_Y + 8);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    disp_printf("D:%d/%d", online, paired);
    disp_printf("   %ddBm", WiFi.RSSI());

    extern bool gateway_ntp_synced(void);
    extern time_t gateway_ntp_epoch(void);
    if (gateway_ntp_synced()) {
        time_t now = gateway_ntp_epoch();
        struct tm *ti = localtime(&now);
        disp_printf("       %02d:%02d", ti->tm_hour, ti->tm_min);
    } else {
        disp_printf("       --:--");
    }

    set_text_color(DISP_WHITE, DISP_BLACK);
}

static void render_page_0(void) {
    cursor(0, 0);
    disp_print("WiFi: ");
    disp_println(WiFi.localIP().toString());
    disp_printf("Ch:%d RSSI:%d", WiFi.channel(), WiFi.RSSI());
    cursor(0, 20);
    disp_print("MQTT: ");
    disp_println(mqtt_client_is_connected() ? "OK" : "---");
    cursor(0, 30);
    extern bool gateway_ntp_synced(void);
    disp_print("NTP: ");
    disp_println(gateway_ntp_synced() ? "sync" : "wait");
}

static void render_page_1(void) {
    cursor(0, 0);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    disp_printf("Devices: %d/%d", online, paired);
    cursor(0, 12);
    char uptime_buf[24];
    uptime_to_str(millis(), uptime_buf, sizeof(uptime_buf));
    disp_printf("Up: %s", uptime_buf);
    cursor(0, 24);
    extern unsigned long espnow_get_rx_count(void);
    extern unsigned long espnow_get_ack_count(void);
    disp_printf("RX: %lu ACK: %lu", espnow_get_rx_count(), espnow_get_ack_count());
    cursor(0, 36);
    if (strlen(s_last_device_name) > 0) {
        disp_printf("Last: %s", s_last_device_name);
    }
}

static void render_page_2(void) {
    cursor(0, 0);
    disp_printf("FW: %s", FW_VERSION);
    cursor(0, 12);
    int t = temperatureRead();
    if (t > 0 && t < 200) {
        disp_printf("Temp: %d C", t);
    }
    cursor(0, 24);
    disp_printf("WiFi RSSI: %d dBm", WiFi.RSSI());
    cursor(0, 36);
    disp_printf("Dev RSSI: %d dBm", s_last_rssi);
}

void display_handler_loop(void) {
    if (!s_display_ok) return;

    unsigned long now = millis();
    if (now - s_last_page_switch < DISPLAY_PAGE_MS) return;
    s_last_page_switch = now;

    s_current_page = (s_current_page + 1) % s_page_count;
    find_newest_device();

    disp_clear();
    switch (s_current_page) {
        case 0: render_page_0(); break;
        case 1: render_page_1(); break;
        case 2: render_page_2(); break;
    }
    render_footer();
    disp_display();
}

#endif
