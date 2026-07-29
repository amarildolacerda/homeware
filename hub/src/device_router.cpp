#include "device_router.h"
#include "sensor_registry.h"
#include "radio_manager.h"
#include "common_console.h"

extern RadioManager s_radio_mgr;

bool device_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    (void)mac;
    return s_radio_mgr.send_command(slot, state);
}

bool device_send_restart(const uint8_t *mac, uint8_t slot) {
    (void)mac;
    return s_radio_mgr.send_restart(slot);
}
