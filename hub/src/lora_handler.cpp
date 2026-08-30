#ifdef LORA_ENABLED

#include "lora_handler.h"
#include "lora_config.h"
#include "common_console.h"
#include "mqtt_client.h"
#include "log_buffer.h"
#include <string.h>

LoraHandler::LoraHandler()
    : m_radio([]{
        LoraSpiConfig cfg;
        cfg.ss = LORA_SS;
        cfg.rst = LORA_RST;
        cfg.dio0 = LORA_DIO0;
        cfg.sck = LORA_SCK;
        cfg.miso = LORA_MISO;
        cfg.mosi = LORA_MOSI;
        cfg.freq = LORA_FREQ;
        cfg.sf = LORA_SF;
        cfg.bw = LORA_BW * 1E3f;
        cfg.cr = LORA_CR;
        cfg.preamble = LORA_PREAMBLE;
        cfg.tx_power = LORA_TX_POWER;
        return cfg;
    }())
{}

int LoraHandler::init() { return m_radio.init(); }
void LoraHandler::loop() {
    m_radio.loop();
    check_offline_sensors();
}
bool LoraHandler::is_ready() const { return m_radio.is_ready(); }

int LoraHandler::send(const uint8_t* data, size_t len) {
    return m_radio.send(data, len);
}

bool LoraHandler::send_command(const uint8_t* mac, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

bool LoraHandler::send_restart(const uint8_t* mac) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = 0xFF;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

void LoraHandler::check_offline_sensors() {
    static unsigned long s_last_timeout_check = 0;
    unsigned long now = millis();
    if (now - s_last_timeout_check < 30000) return;
    s_last_timeout_check = now;

    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && s->radio_type == RADIO_LORA && s->online) {
            unsigned long elapsed = now - s->last_seen;
            if (elapsed > SENSOR_TIMEOUT_MS) {
                s->online = false;
                log_add("warn", "LoRa sensor slot %d offline", i);
                console.printf("[LoRa] Sensor slot %d offline (last seen %lu ms ago)\n", i, elapsed);
                if (mqtt_client_is_connected()) {
                    mqtt_client_publish_availability(s, false);
                }
            }
        }
    }
}

#endif
