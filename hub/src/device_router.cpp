#include "device_router.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "lora_protocol.h"
#include "common_console.h"

#ifdef HABILITA_LORA
#include "lora_handler.h"
extern LoraHandler s_lora;

static void lora_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    s_lora.send((const uint8_t*)&cmd, sizeof(cmd));
    console.printf("[LoRa] Command sent to slot %d state=%d\n", slot, state);
}
#endif

bool device_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || !s->paired) return false;

    if (s->radio_type == RADIO_LORA) {
#ifdef HABILITA_LORA
        lora_send_command(mac, slot, state);
        return true;
#else
        return false;
#endif
    }
    return espnow_send_command(mac, slot, state);
}

bool device_send_restart(const uint8_t *mac, uint8_t slot) {
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || !s->paired) return false;

    if (s->radio_type == RADIO_LORA) {
        console.printf("[LoRa] Restart not supported for LoRa devices\n");
        return false;
    }
    return espnow_send_restart(mac, slot);
}
