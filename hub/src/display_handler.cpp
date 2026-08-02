#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)

#include "display_handler.h"
#include "page_manager.h"
#include "config.h"
#include "sensor_registry.h"
#include "platform.h"
#include "common_console.h"
#include "mqtt_client.h"
#include "radio_manager.h"
#include "common_util.h"
#include <time.h>

extern RadioManager s_radio_mgr;

#if defined(DISPLAY_TTGO)
#include "display_ttgo.h"
static Ssd1306DisplayTtgo s_display;
#elif defined(DISPLAY_HELTEC)
#include "display_heltec.h"
static Ssd1306DisplayHeltec s_display;
#endif

static PageManager s_pages(&s_display);
static bool s_display_ok = false;
static int s_last_rssi = 0;
static char s_last_device_name[32] = "";
static char s_chip_id_str[16];
static char s_platform_str[16];

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

static void render_page_0(DisplayInterface& d) {
    d.set_cursor(0, 0);
    d.print("WiFi: ");
    d.print(WiFi.localIP().toString().c_str());
    d.set_cursor(0, 12);
    d.printf("Ch:%d RSSI:%d", WiFi.channel(), WiFi.RSSI());
    d.set_cursor(0, 24);
    d.print("MQTT: ");
    d.print(mqtt_client_is_connected() ? "OK" : "---");
    d.set_cursor(0, 36);
    d.print("NTP: ");
    extern bool gateway_ntp_synced(void);
    d.print(gateway_ntp_synced() ? "sync" : "wait");
}

static void render_page_1(DisplayInterface& d) {
    d.set_cursor(0, 0);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    d.printf("Devices: %d/%d", online, paired);
    d.set_cursor(0, 12);
    char uptime_buf[24];
    uptime_to_str(millis(), uptime_buf, sizeof(uptime_buf));
    d.printf("Up: %s", uptime_buf);
    d.set_cursor(0, 24);
    d.printf("RX: %lu ACK: %lu", s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count());
    d.set_cursor(0, 36);
    if (strlen(s_last_device_name) > 0) {
        d.printf("Last: %s", s_last_device_name);
    }
}

static void render_page_2(DisplayInterface& d) {
    d.set_cursor(0, 0);
    d.printf("FW: %s", FW_VERSION);
    d.set_cursor(0, 12);
    int t = temperatureRead();
    if (t > 0 && t < 200) {
        d.printf("Temp: %d C", t);
    }
    d.set_cursor(0, 24);
    d.printf("WiFi RSSI: %d dBm", WiFi.RSSI());
    d.set_cursor(0, 36);
    d.printf("Dev RSSI: %d dBm", s_last_rssi);
}

static void render_footer(DisplayInterface& d) {
    d.set_cursor(0, 52);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    d.printf("D:%d/%d", online, paired);
    d.printf("   %ddBm", WiFi.RSSI());
    extern bool gateway_ntp_synced(void);
    extern time_t gateway_ntp_epoch(void);
    if (gateway_ntp_synced()) {
        time_t now = gateway_ntp_epoch();
        struct tm *ti = localtime(&now);
        d.printf("       %02d:%02d", ti->tm_hour, ti->tm_min);
    } else {
        d.printf("       --:--");
    }
}

void display_handler_init(void) {
    if (!s_display.begin()) {
        console.printf("[Display] begin() FAIL\n");
        return;
    }
    s_display_ok = true;
    console.printf("[Display] begin() OK\n");

    s_display.set_text_size(1);
    s_display.clear();
    s_display.print("Booting...");
    s_display.display();

    snprintf(s_chip_id_str, sizeof(s_chip_id_str), "0x%06x", chip_id());
    snprintf(s_platform_str, sizeof(s_platform_str), "%s", PLATFORM_PREFIX);

    s_pages.add_page(render_page_0);
    s_pages.add_page(render_page_1);
    s_pages.add_page(render_page_2);
    s_pages.set_footer(render_footer);
}

void display_handler_loop(void) {
    if (!s_display_ok) return;
    find_newest_device();
    s_pages.loop();
}

#endif
