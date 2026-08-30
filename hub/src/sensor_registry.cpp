#include "sensor_registry.h"
#include "config.h"
#include "config_store.h"
#include <Arduino.h>
#include "common_console.h"

static virtual_sensor_t s_sensors[MAX_VIRTUAL_SENSORS];
static bool s_initialized = false;
static bool s_dirty = false;

bool sensor_registry_init() {
    config_store_init();
    sensor_registry_load();
    s_initialized = true;
    console.printf("[Gateway] Sensor registry initialized: %d paired\n", sensor_registry_count_paired());
    return true;
}

int sensor_registry_find_by_mac(const uint8_t *mac) {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (mac_equal(s_sensors[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

int sensor_registry_find_by_name(const char *name) {
    if (!name || name[0] == '\0') return -1;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired && strcmp(s_sensors[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int sensor_registry_find_free_slot() {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (!s_sensors[i].paired) {
            return i;
        }
    }
    return -1;
}

virtual_sensor_t* sensor_registry_get(int slot) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return nullptr;
    return &s_sensors[slot];
}

int sensor_registry_count_paired() {
    int count = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired) count++;
    }
    return count;
}

int sensor_registry_count_online() {
    int count = 0;
    unsigned long now = millis();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired && s_sensors[i].online && (now - s_sensors[i].last_seen < SENSOR_TIMEOUT_MS)) {
            count++;
        }
    }
    return count;
}

bool sensor_registry_add(const uint8_t *mac, uint8_t type, uint16_t slot, const char *name, uint8_t client_chip, uint8_t radio_type) {
    if (slot >= MAX_VIRTUAL_SENSORS) return false;
    if (sensor_registry_find_by_mac(mac) >= 0) return false;

    virtual_sensor_t *s = &s_sensors[slot];
    mac_copy(s->mac, mac);
    s->type = type;
    s->slot = slot;
    s->client_chip = client_chip;
    s->radio_type = radio_type;
    s->sequence = 0;
    s->battery_pct = 100;
    s->last_rssi = -127;
    s->last_seen = 0;
    s->paired = true;
    s->online = false;
    memset(&s->state, 0, sizeof(s->state));
    snprintf(s->bridge_device_id, sizeof(s->bridge_device_id), "gw_%02X%02X%02X_%d", mac[3], mac[4], mac[5], slot);
    if (name && strlen(name) > 0) {
        strncpy(s->name, name, sizeof(s->name) - 1);
    } else {
        snprintf(s->name, sizeof(s->name), "%s %d", sensor_type_friendly_name(s->type), slot + 1);
    }
    s->name[sizeof(s->name) - 1] = '\0';

    s_dirty = true;
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[Gateway] Added sensor slot %d: MAC=%s type=%d\n",
                  slot, mac_str, type);
    return true;
}

bool sensor_registry_remove(int slot) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) {
        console.printf("[Gateway] Remove slot %d: invalid (max=%d)\n", slot, MAX_VIRTUAL_SENSORS);
        return false;
    }
    if (!s_sensors[slot].paired) {
        console.printf("[Gateway] Remove slot %d: not paired (name=%s bid=%s)\n",
                       slot, s_sensors[slot].name, s_sensors[slot].bridge_device_id);
        return false;
    }

    console.printf("[Gateway] Removing sensor slot %d: %s (%s)\n",
                   slot, s_sensors[slot].name, s_sensors[slot].bridge_device_id);
    memset(&s_sensors[slot], 0, sizeof(virtual_sensor_t));
    s_dirty = true;
    return true;
}

bool sensor_registry_update_state(int slot, const espnow_header_t *header, const uint8_t *payload, size_t payload_len) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return false;
    virtual_sensor_t *s = &s_sensors[slot];
    if (!s->paired) return false;

    s->sequence = header->sequence;
    s->battery_pct = header->battery_pct;
    s->last_rssi = header->rssi;
    s->last_seen = millis();
    s->online = true;
    // Extract node IP from ESP-NOW header (v2+)
    if (header->ip[0] != 0 || header->ip[1] != 0 || header->ip[2] != 0 || header->ip[3] != 0) {
        memcpy(s->ip, header->ip, 4);
    }

    switch (s->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            if (payload_len >= sizeof(payload_temp_hum_t)) {
                payload_temp_hum_t *p = (payload_temp_hum_t*)payload;
                s->state.temp_hum.temperature = p->temperature;
                s->state.temp_hum.humidity = p->humidity;
            }
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            if (payload_len >= sizeof(payload_contact_t)) {
                payload_contact_t *p = (payload_contact_t*)payload;
                s->state.contact.contact_state = p->contact_state;
                s->state.contact.tamper = p->tamper;
            }
            break;
        }
        case SENSOR_TYPE_MOTION: {
            if (payload_len >= sizeof(payload_motion_t)) {
                payload_motion_t *p = (payload_motion_t*)payload;
                s->state.motion.motion_state = p->motion_state;
                s->state.motion.occupancy_duration = p->occupancy_duration;
            }
            break;
        }
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS: {
            if (payload_len >= sizeof(payload_dht_gas_t)) {
                payload_dht_gas_t *p = (payload_dht_gas_t*)payload;
                s->state.dht_gas.temperature = p->temperature;
                s->state.dht_gas.humidity = p->humidity;
                s->state.dht_gas.gas_level = p->gas_level;
                s->state.dht_gas.alarm = p->alarm;
            } else if (payload_len >= sizeof(payload_gas_t)) {
                payload_gas_t *p = (payload_gas_t*)payload;
                s->state.dht_gas.gas_level = p->gas_level;
                s->state.dht_gas.alarm = p->alarm;
            }
            break;
        }
        case SENSOR_TYPE_RAIN: {
            if (payload_len >= sizeof(payload_rain_t)) {
                payload_rain_t *p = (payload_rain_t*)payload;
                s->state.rain.rain_level = p->rain_level;
                s->state.rain.rain_digital = p->rain_digital;
            }
            break;
        }
        case SENSOR_TYPE_LEVEL: {
            if (payload_len >= sizeof(payload_tank_t)) {
                payload_tank_t *p = (payload_tank_t*)payload;
                s->state.tank.level_pct = p->level_pct;
                s->state.tank.distance_cm = p->distance_cm;
            }
            break;
        }
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT: {
            if (payload_len >= sizeof(payload_onoff_t)) {
                payload_onoff_t *p = (payload_onoff_t*)payload;
                s->state.onoff.state = p->state;
            }
            break;
        }
        case SENSOR_TYPE_REPEATER: {
            if (payload_len >= sizeof(payload_repeater_status_t)) {
                payload_repeater_status_t *p = (payload_repeater_status_t*)payload;
                s->state.repeater.received = p->received;
                s->state.repeater.forwarded = p->forwarded;
                s->state.repeater.client_count = p->client_count;
                s->state.repeater.channel = p->channel;
                s->state.repeater.uptime_s = p->uptime_s;
                s->state.repeater.free_heap = p->free_heap;
                s->state.repeater.ack_failures = p->ack_failures;
            }
            break;
        }
        case SENSOR_TYPE_SOIL_MOISTURE:
        {
            if (payload_len < sizeof(payload_soil_moisture_t)) return false;
            payload_soil_moisture_t *pl = (payload_soil_moisture_t *)payload;
            s->state.soil_moisture.raw_adc = pl->raw_adc;
            s->state.soil_moisture.moisture_pct = pl->moisture_pct;
            break;
        }
    }

    size_t expected = 0;
    switch (s->type) {
        case SENSOR_TYPE_TEMP_HUM: expected = sizeof(payload_temp_hum_t); break;
        case SENSOR_TYPE_CONTACT:  expected = sizeof(payload_contact_t); break;
        case SENSOR_TYPE_MOTION:   expected = sizeof(payload_motion_t); break;
        case SENSOR_TYPE_GAS:      expected = sizeof(payload_gas_t); break;
        case SENSOR_TYPE_DHT_GAS:  expected = sizeof(payload_dht_gas_t); break;
        case SENSOR_TYPE_RAIN:     expected = sizeof(payload_rain_t); break;
        case SENSOR_TYPE_LEVEL:     expected = sizeof(payload_tank_t); break;
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT:    expected = sizeof(payload_onoff_t); break;
        case SENSOR_TYPE_REPEATER: expected = sizeof(payload_repeater_status_t); break;
        case SENSOR_TYPE_SOIL_MOISTURE: expected = sizeof(payload_soil_moisture_t); break;
    }
    if (expected && payload_len >= expected + 6) {
        memcpy(s->ip, payload + payload_len - 6, 4);
        memcpy(&s->free_heap, payload + payload_len - 2, sizeof(s->free_heap));
    } else if (expected && payload_len >= expected + 4) {
        memcpy(s->ip, payload + payload_len - 4, 4);
    }

    return true;
}

bool sensor_registry_save() {
    SensorSlot slots[MAX_VIRTUAL_SENSORS];
    memset(slots, 0, sizeof(slots));

    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        slots[i].paired      = s_sensors[i].paired;
        slots[i].sensor_type = s_sensors[i].type;
        slots[i].slot        = s_sensors[i].slot;
        memcpy(slots[i].mac, s_sensors[i].mac, 6);
        strncpy(slots[i].name, s_sensors[i].name, sizeof(slots[i].name) - 1);
        slots[i].client_chip = s_sensors[i].client_chip;
        slots[i].radio_type  = s_sensors[i].radio_type;
        strncpy(slots[i].bridge_device_id, s_sensors[i].bridge_device_id, sizeof(slots[i].bridge_device_id) - 1);
    }

    bool ok = config_sensors_save(slots, MAX_VIRTUAL_SENSORS);
    console.printf("[CFG] Sensors saved: %s (%d paired)\n", ok ? "OK" : "FAIL", sensor_registry_count_paired());
    s_dirty = false;
    return ok;
}

void sensor_registry_flush_if_dirty() {
    if (s_dirty) sensor_registry_save();
}

void sensor_registry_load() {
    SensorSlot slots[MAX_VIRTUAL_SENSORS];
    if (!config_sensors_load(slots, MAX_VIRTUAL_SENSORS)) {
        console.println("[CFG] No saved sensors found");
        return;
    }

    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (!slots[i].paired) continue;

        virtual_sensor_t *s = &s_sensors[i];
        s->paired      = true;
        s->type        = slots[i].sensor_type;
        s->slot        = slots[i].slot;
        memcpy(s->mac, slots[i].mac, 6);
        strncpy(s->name, slots[i].name, sizeof(s->name) - 1);
        s->client_chip = slots[i].client_chip;
        s->radio_type  = slots[i].radio_type;
        s->sequence    = 0;
        s->battery_pct = 100;
        s->last_rssi   = -127;
        s->last_seen   = 0;
        s->online      = false;
        memset(&s->state, 0, sizeof(s->state));

        if (strlen(slots[i].bridge_device_id) > 0) {
            strncpy(s->bridge_device_id, slots[i].bridge_device_id, sizeof(s->bridge_device_id) - 1);
        } else {
            snprintf(s->bridge_device_id, sizeof(s->bridge_device_id),
                     "gw_%02X%02X%02X_%d", s->mac[3], s->mac[4], s->mac[5], i);
        }

        console.printf("[CFG] Loaded slot %d: %s (type=%d)\n", i, s->name, s->type);
    }
}

void sensor_registry_clear_all() {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        memset(&s_sensors[i], 0, sizeof(virtual_sensor_t));
    }
    s_dirty = true;
    console.println("[Gateway] All sensors cleared");
}

void sensor_registry_print_all() {
    console.println("\n=== Sensor Registry ===");
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired) {
            char mac_str[18];
            mac_to_str(s_sensors[i].mac, mac_str, sizeof(mac_str));
            console.printf("  Slot %d: %s | MAC=%s | Type=%d | Seq=%u | Batt=%d%% | RSSI=%d | Online=%s\n",
                          i, s_sensors[i].name, mac_str, s_sensors[i].type,
                          s_sensors[i].sequence, s_sensors[i].battery_pct,
                          s_sensors[i].last_rssi, s_sensors[i].online ? "Yes" : "No");
        }
    }
    console.printf("Total: %d paired, %d online\n", sensor_registry_count_paired(), sensor_registry_count_online());
    console.println("========================\n");
}

const char* sensor_type_friendly_name(uint8_t type) {
    switch (type) {
        case SENSOR_TYPE_TEMP_HUM: return "Temp+Hum";
        case SENSOR_TYPE_CONTACT: return "Contato";
        case SENSOR_TYPE_MOTION: return "Movimento";
        case SENSOR_TYPE_GAS: return "Gas";
        case SENSOR_TYPE_DHT_GAS: return "DHT+Gas";
        case SENSOR_TYPE_RAIN: return "Chuva";
        case SENSOR_TYPE_ONOFF: return "Interruptor";
        case SENSOR_TYPE_LIGHT: return "Lâmpada";
        case SENSOR_TYPE_LEVEL: return "Tanque";
        case SENSOR_TYPE_REPEATER: return "Repeater";
        case SENSOR_TYPE_SOIL_MOISTURE: return "Solo";
        default: return "Sensor";
    }
}

const char* sensor_type_to_string(uint8_t type) {
    switch (type) {
        case SENSOR_TYPE_TEMP_HUM: return "temperature";
        case SENSOR_TYPE_CONTACT: return "contact";
        case SENSOR_TYPE_MOTION: return "occupancy";
        case SENSOR_TYPE_GAS: return "gas";
        case SENSOR_TYPE_DHT_GAS: return "dht_gas";
        case SENSOR_TYPE_RAIN: return "rain";
        case SENSOR_TYPE_ONOFF: return "onoff";
        case SENSOR_TYPE_LIGHT: return "light";
        case SENSOR_TYPE_LEVEL: return "tanque";
        case SENSOR_TYPE_REPEATER: return "repeater";
        case SENSOR_TYPE_SOIL_MOISTURE: return "soil_moisture";
        default: return "unknown";
    }
}
