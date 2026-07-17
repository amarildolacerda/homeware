#include "common_wifi.h"

bool common_wifi_begin(bool force_portal) {
    return mywifi_begin(force_portal);
}

void common_wifi_loop() {
    mywifi_loop();
}
