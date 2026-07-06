#include "mqtt_client.h"
#include "config.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#define MQTT_MAX_PACKET_SIZE 768
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient s_wifi_client;
static PubSubClient s_mqtt(s_wifi_client);

static char s_mqtt_host[64] = MQTT_HOST_DEFAULT;
static uint16_t s_mqtt_port = MQTT_PORT_DEFAULT;
static char s_mqtt_user[32] = "";
static char s_mqtt_pass[32] = "";
static bool s_mqtt_connected = false;
static unsigned long s_last_reconnect = 0;
static bool s_should_reconnect = true;

#define MQTT_TOPIC_PREFIX "homeassistant"
#define MQTT_RECONNECT_INTERVAL 30000

static bool is_valid_host() {
    if (strcmp(s_mqtt_host, "0.0.0.0") == 0) return false;
    if (strlen(s_mqtt_host) == 0) return false;
    return true;
}

static void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    char buf[128];
    unsigned int len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, len);
    buf[len] = '\0';

    String topicStr(topic);
    if (!topicStr.startsWith(MQTT_TOPIC_PREFIX "/switch/") || !topicStr.endsWith("/set")) return;

    int id_start = String(MQTT_TOPIC_PREFIX "/switch/").length();
    int id_end = topicStr.lastIndexOf("/set");
    if (id_start >= id_end) return;
    String entity_id = topicStr.substring(id_start, id_end);

    const char suffix[] = "_power";
    if (!entity_id.endsWith(suffix)) return;
    String dev_id = entity_id.substring(0, entity_id.length() - strlen(suffix));

    uint8_t state = 0;
    if (strcmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0) {
        state = 1;
    }

    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strcmp(s->bridge_device_id, dev_id.c_str()) == 0) {
            espnow_send_command(s->mac, s->slot, state);
            Serial.printf("[MQTT] Command forwarded: %s -> slot %d state=%d\n", dev_id.c_str(), i, state);
            break;
        }
    }
}

static void publish_entity_config(const char *component, const char *entity_id,
                                  const char *sensor_name, const char *entity_label,
                                  const char *device_class, const char *unit, bool is_binary) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/%s/config", MQTT_TOPIC_PREFIX, component, entity_id);

    JsonDocument doc;
    char friendly[64];
    snprintf(friendly, sizeof(friendly), "%s - %s", sensor_name, entity_label);
    doc["name"] = friendly;
    doc["state_topic"] = String(MQTT_TOPIC_PREFIX "/") + component + "/" + entity_id + "/state";
    doc["unique_id"] = entity_id;

    if (is_binary) {
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
    }

    if (strcmp(component, "switch") == 0) {
        doc["command_topic"] = String(MQTT_TOPIC_PREFIX "/switch/") + entity_id + "/set";
    }

    if (strlen(device_class) > 0) {
        doc["device_class"] = device_class;
    }
    if (strlen(unit) > 0) {
        doc["unit_of_measurement"] = unit;
    }

    String json;
    serializeJson(doc, json);
    s_mqtt.publish(topic, json.c_str(), true);
}

static void publish_entity_state(const char *component, const char *entity_id, const char *value) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/%s/state", MQTT_TOPIC_PREFIX, component, entity_id);
    s_mqtt.publish(topic, value, false);
}

bool mqtt_client_load_config() {
    EEPROM.begin(EEPROM_SIZE);

    bool valid = false;
    for (int i = 0; i < 63; i++) {
        char c = EEPROM.read(EEPROM_MQTT_HOST_OFFSET + i);
        if (c == 0) { valid = true; break; }
        s_mqtt_host[i] = c;
    }
    s_mqtt_host[63] = '\0';
    if (!valid || strlen(s_mqtt_host) == 0) {
        strcpy(s_mqtt_host, MQTT_HOST_DEFAULT);
    }

    uint16_t port = 0;
    EEPROM.get(EEPROM_MQTT_PORT_OFFSET, port);
    if (port > 0 && port < 65535) {
        s_mqtt_port = port;
    }

    valid = false;
    for (int i = 0; i < 31; i++) {
        char c = EEPROM.read(EEPROM_MQTT_USER_OFFSET + i);
        if (c == 0) { valid = true; break; }
        s_mqtt_user[i] = c;
    }
    s_mqtt_user[31] = '\0';
    if (!valid) s_mqtt_user[0] = '\0';

    valid = false;
    for (int i = 0; i < 31; i++) {
        char c = EEPROM.read(EEPROM_MQTT_PASS_OFFSET + i);
        if (c == 0) { valid = true; break; }
        s_mqtt_pass[i] = c;
    }
    s_mqtt_pass[31] = '\0';
    if (!valid) s_mqtt_pass[0] = '\0';

    EEPROM.end();

    Serial.printf("[MQTT] Config loaded: %s:%d user='%s'\n", s_mqtt_host, s_mqtt_port, s_mqtt_user);
    return true;
}

bool mqtt_client_save_config(const char *host, uint16_t port, const char *user, const char *pass) {
    EEPROM.begin(EEPROM_SIZE);

    for (int i = 0; i < 64; i++) {
        EEPROM.write(EEPROM_MQTT_HOST_OFFSET + i, i < (int)strlen(host) ? host[i] : 0);
    }
    EEPROM.put(EEPROM_MQTT_PORT_OFFSET, port);

    for (int i = 0; i < 32; i++) {
        EEPROM.write(EEPROM_MQTT_USER_OFFSET + i, i < (int)strlen(user) ? user[i] : 0);
    }
    for (int i = 0; i < 32; i++) {
        EEPROM.write(EEPROM_MQTT_PASS_OFFSET + i, i < (int)strlen(pass) ? pass[i] : 0);
    }

    EEPROM.commit();
    EEPROM.end();

    strcpy(s_mqtt_host, host);
    s_mqtt_port = port;
    strcpy(s_mqtt_user, user);
    strcpy(s_mqtt_pass, pass);

    s_should_reconnect = true;
    Serial.printf("[MQTT] Config saved: %s:%d user='%s'\n", host, port, user);
    return true;
}

bool mqtt_client_connect() {
    if (!is_valid_host()) return false;

    s_mqtt.setServer(s_mqtt_host, s_mqtt_port);
    s_mqtt.setCallback(mqtt_callback);

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "gateway_%06x", ESP.getChipId());

    bool ok = false;
    if (strlen(s_mqtt_user) > 0) {
        ok = s_mqtt.connect(client_id, s_mqtt_user, s_mqtt_pass);
    } else {
        ok = s_mqtt.connect(client_id);
    }

    if (ok) {
        s_mqtt_connected = true;
        s_should_reconnect = false;
        Serial.printf("[MQTT] Connected to %s:%d\n", s_mqtt_host, s_mqtt_port);

        s_mqtt.subscribe("homeassistant/switch/+/set");

        mqtt_client_publish_all();
    } else {
        Serial.printf("[MQTT] Connection failed rc=%d\n", s_mqtt.state());
    }

    return ok;
}

void mqtt_client_disconnect() {
    s_mqtt.disconnect();
    s_mqtt_connected = false;
}

void mqtt_client_loop() {
    if (!is_valid_host()) return;

    if (!s_mqtt.connected()) {
        s_mqtt_connected = false;
        unsigned long now = millis();
        if (now - s_last_reconnect > MQTT_RECONNECT_INTERVAL || s_should_reconnect) {
            s_last_reconnect = now;
            mqtt_client_connect();
        }
    } else {
        s_mqtt.loop();
    }
}

bool mqtt_client_is_connected() { return s_mqtt_connected; }
const char* mqtt_client_get_host() { return s_mqtt_host; }
uint16_t mqtt_client_get_port() { return s_mqtt_port; }
const char* mqtt_client_get_user() { return s_mqtt_user; }
const char* mqtt_client_get_pass() { return s_mqtt_pass; }

const char* get_gateway_device_id() {
    static char id[48];
    uint32_t chip_id = ESP.getChipId();
    snprintf(id, sizeof(id), "esp8266_gateway_%06x", chip_id);
    return id;
}

void mqtt_client_generate_device_ids() {
    uint32_t chip_id = ESP.getChipId();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) == 0) {
            snprintf(s->bridge_device_id, sizeof(s->bridge_device_id),
                     "esp8266_%06x_sensor_%d", chip_id, i);
        }
    }
}

bool mqtt_client_publish_discovery(virtual_sensor_t *sensor) {
    if (!s_mqtt_connected) return false;

    const char *id = sensor->bridge_device_id;
    const char *name = sensor->name;

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_temperature", id);
            publish_entity_config("sensor", entity, name, "Temperatura", "temperature", "°C", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.temp_hum.temperature, 1).c_str());

            snprintf(entity, sizeof(entity), "%s_humidity", id);
            publish_entity_config("sensor", entity, name, "Umidade", "humidity", "%", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.temp_hum.humidity, 0).c_str());
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_contact", id);
            publish_entity_config("binary_sensor", entity, name, "Contato", "door", "", true);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.contact.contact_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_MOTION: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_occupancy", id);
            publish_entity_config("binary_sensor", entity, name, "Movimento", "occupancy", "", true);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.motion.motion_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_GAS: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_gas_level", id);
            publish_entity_config("sensor", entity, name, "Gás", "gas", "ppm", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.gas.gas_level).c_str());

            snprintf(entity, sizeof(entity), "%s_alarm", id);
            publish_entity_config("binary_sensor", entity, name, "Alarme", "smoke", "", true);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_DHT_GAS: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_temperature", id);
            publish_entity_config("sensor", entity, name, "Temperatura", "temperature", "°C", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.temperature, 1).c_str());

            snprintf(entity, sizeof(entity), "%s_humidity", id);
            publish_entity_config("sensor", entity, name, "Umidade", "humidity", "%", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.humidity, 0).c_str());

            snprintf(entity, sizeof(entity), "%s_gas_level", id);
            publish_entity_config("sensor", entity, name, "Gás", "gas", "ppm", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.gas_level).c_str());

            snprintf(entity, sizeof(entity), "%s_alarm", id);
            publish_entity_config("binary_sensor", entity, name, "Alarme", "smoke", "", true);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.dht_gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_RAIN: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_rain_level", id);
            publish_entity_config("sensor", entity, name, "Chuva", "moisture", "%", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.rain.rain_level).c_str());

            snprintf(entity, sizeof(entity), "%s_rain", id);
            publish_entity_config("binary_sensor", entity, name, "Chuva Digital", "moisture", "", true);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.rain.rain_digital ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_TANK: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_level", id);
            publish_entity_config("sensor", entity, name, "Nível", "water", "%", false);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.tank.level_pct).c_str());
            break;
        }
        case SENSOR_TYPE_ONOFF: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_power", id);
            publish_entity_config("switch", entity, name, "Interruptor", "", "", false);
            publish_entity_state("switch", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
    }

    return true;
}

bool mqtt_client_publish_state(virtual_sensor_t *sensor) {
    if (!s_mqtt_connected) return false;

    const char *id = sensor->bridge_device_id;

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            char entity[64], val[16];
            snprintf(entity, sizeof(entity), "%s_temperature", id);
            snprintf(val, sizeof(val), "%.1f", sensor->state.temp_hum.temperature);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_humidity", id);
            snprintf(val, sizeof(val), "%.0f", sensor->state.temp_hum.humidity);
            publish_entity_state("sensor", entity, val);
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_contact", id);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.contact.contact_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_MOTION: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_occupancy", id);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.motion.motion_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_GAS: {
            char entity[64], val[16];
            snprintf(entity, sizeof(entity), "%s_gas_level", id);
            snprintf(val, sizeof(val), "%u", sensor->state.gas.gas_level);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_alarm", id);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_DHT_GAS: {
            char entity[64], val[16];
            snprintf(entity, sizeof(entity), "%s_temperature", id);
            snprintf(val, sizeof(val), "%.1f", sensor->state.dht_gas.temperature);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_humidity", id);
            snprintf(val, sizeof(val), "%.0f", sensor->state.dht_gas.humidity);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_gas_level", id);
            snprintf(val, sizeof(val), "%u", sensor->state.dht_gas.gas_level);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_alarm", id);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.dht_gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_RAIN: {
            char entity[64], val[16];
            snprintf(entity, sizeof(entity), "%s_rain_level", id);
            snprintf(val, sizeof(val), "%u", sensor->state.rain.rain_level);
            publish_entity_state("sensor", entity, val);

            snprintf(entity, sizeof(entity), "%s_rain", id);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.rain.rain_digital ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_TANK: {
            char entity[64], val[16];
            snprintf(entity, sizeof(entity), "%s_level", id);
            snprintf(val, sizeof(val), "%u", sensor->state.tank.level_pct);
            publish_entity_state("sensor", entity, val);
            break;
        }
        case SENSOR_TYPE_ONOFF: {
            char entity[64];
            snprintf(entity, sizeof(entity), "%s_power", id);
            publish_entity_state("switch", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
    }

    return true;
}

bool mqtt_client_publish_all() {
    if (!s_mqtt_connected) return false;

    int count = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) > 0) {
            if (mqtt_client_publish_discovery(s)) count++;
        }
    }
    Serial.printf("[MQTT] Published %d sensors to MQTT\n", count);
    return count > 0;
}
