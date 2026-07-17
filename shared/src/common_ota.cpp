#include "common_ota.h"
#include <ArduinoOTA.h>
#include "common_console.h"

void ota_setup(const char *hostname) {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() { console.println("OTA: start"); });
    ArduinoOTA.onEnd([]() { console.println("\nOTA: end"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint32_t last = 0;
        uint32_t now = millis();
        if (now - last > 500) {
            last = now;
            console.printf("OTA: %u%%\r", (progress * 100) / total);
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        console.printf("OTA: erro %d\n", error);
    });
    ArduinoOTA.begin();
    console.printf("OTA pronto: %s.local\n", hostname);
}

void ota_handle() {
    ArduinoOTA.handle();
}
