#include "mqtt_client.h"
#include "config.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "platform.h"
#include <EEPROM.h>
#define MQTT_MAX_PACKET_SIZE 768
#include "log_buffer.h"
#include "common_console.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient s_wifi_client;
static PubSubClient s_mqtt(s_wifi_client);

static char s_mqtt_host[64] = MQTT_HOST_DEFAULT;
static uint16_t s_mqtt_port = MQTT_PORT_DEFAULT;
static char s_mqtt_user[32] = MQTT_USER_DEFAULT;
static char s_mqtt_pass[32] = MQTT_PASS_DEFAULT;
static bool s_mqtt_connected = false;
static unsigned long s_mqtt_connected_since = 0;
static unsigned long s_last_reconnect = 0;
static bool s_should_reconnect = true;

#define MQTT_TOPIC_PREFIX "homeassistant"
#define MQTT_RECONNECT_INTERVAL 30000
#define GW_MANUFACTURER "ESP-HA Bridge"

static void build_device_info(JsonDocument &doc, const char *name, const char *bridge_id, const char *model) {
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(bridge_id);
    device["name"] = name;
    device["sw_version"] = FW_VERSION;
    device["manufacturer"] = GW_MANUFACTURER;
    device["model"] = model;
    device["suggested_area"] = "matter";
}

static void build_entity_id(char *buf, size_t len, const uint8_t *mac, int slot, const char *suffix) {
    snprintf(buf, len, "gw_%02X%02X%02X_%s_%d", mac[3], mac[4], mac[5], suffix, slot);
}

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

    console.printf("[MQTT RX] topic=%s payload=%s (len=%u)\n", topic, buf, length);

    String topicStr(topic);
    bool is_switch = topicStr.startsWith(MQTT_TOPIC_PREFIX "/switch/");
    bool is_light = topicStr.startsWith(MQTT_TOPIC_PREFIX "/light/");
    if ((!is_switch && !is_light) || !topicStr.endsWith("/set")) return;

    int id_start = is_switch ? String(MQTT_TOPIC_PREFIX "/switch/").length()
                             : String(MQTT_TOPIC_PREFIX "/light/").length();
    int id_end = topicStr.lastIndexOf("/set");
    if (id_start >= id_end) return;
    String entity_id = topicStr.substring(id_start, id_end);

    int sep = entity_id.lastIndexOf('_');
    if (sep < 0) return;
    int slot = atoi(entity_id.c_str() + sep + 1);

    uint8_t state = 0;
    if (strcmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0) {
        state = 1;
    }

    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && s->slot == slot) {
            espnow_send_command(s->mac, s->slot, state);
            console.printf("[MQTT] Command forwarded: slot %d state=%d\n", i, state);
            break;
        }
    }
}

static void publish_entity_config(const char *component, const char *entity_id,
                                  const char *sensor_name, const char *entity_label,
                                  const char *device_class, const char *unit, bool is_binary,
                                  const char *bridge_id, const char *model) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/%s/config", MQTT_TOPIC_PREFIX, component, entity_id);

    JsonDocument doc;
    doc["name"] = entity_label;
    doc["state_topic"] = String(MQTT_TOPIC_PREFIX "/") + component + "/" + entity_id + "/state";
    doc["unique_id"] = entity_id;

    build_device_info(doc, sensor_name, bridge_id, model);

    if (is_binary) {
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
    }

    if (strcmp(component, "switch") == 0 || strcmp(component, "light") == 0) {
        doc["command_topic"] = String(MQTT_TOPIC_PREFIX "/") + component + "/" + entity_id + "/set";
    }

    if (strlen(device_class) > 0) {
        doc["device_class"] = device_class;
    }
    if (strlen(unit) > 0) {
        doc["unit_of_measurement"] = unit;
    }

    String json;
    serializeJson(doc, json);
    console.printf("[MQTT TX] topic=%s payload=%s\n", topic, json.c_str());
    if (s_mqtt.beginPublish(topic, json.length(), true)) {
        s_mqtt.print(json.c_str());
        s_mqtt.endPublish();
    }
}

static void publish_entity_state(const char *component, const char *entity_id, const char *value) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/%s/state", MQTT_TOPIC_PREFIX, component, entity_id);
    console.printf("[MQTT TX] topic=%s payload=%s\n", topic, value);
    s_mqtt.publish(topic, value, false);
}

// Load a NUL-terminated printable-ASCII string from EEPROM. Returns false
// (and leaves buf empty) if the region holds garbage: no terminator within
// buflen, or any non-printable byte before the terminator. This prevents
// corrupted EEPROM data from leaking control characters into JSON responses.
static bool eeprom_read_string(uint16_t offset, char *buf, size_t buflen) {
    bool terminated = false;
    for (size_t i = 0; i < buflen - 1; i++) {
        char c = EEPROM.read(offset + i);
        if (c == 0) { terminated = true; break; }
        if (c < 0x20 || c > 0x7E) { terminated = false; break; }
        buf[i] = c;
    }
    buf[buflen - 1] = '\0';
    return terminated && strlen(buf) > 0;
}

bool mqtt_client_load_config() {
    EEPROM.begin(EEPROM_SIZE);

    if (!eeprom_read_string(EEPROM_MQTT_HOST_OFFSET, s_mqtt_host, sizeof(s_mqtt_host))) {
        strcpy(s_mqtt_host, MQTT_HOST_DEFAULT);
    }

    uint16_t port = 0;
    EEPROM.get(EEPROM_MQTT_PORT_OFFSET, port);
    if (port > 0 && port < 65535) {
        s_mqtt_port = port;
    }

    if (!eeprom_read_string(EEPROM_MQTT_USER_OFFSET, s_mqtt_user, sizeof(s_mqtt_user))) {
        s_mqtt_user[0] = '\0';
    }

    if (!eeprom_read_string(EEPROM_MQTT_PASS_OFFSET, s_mqtt_pass, sizeof(s_mqtt_pass))) {
        s_mqtt_pass[0] = '\0';
    }

    EEPROM.end();

    console.printf("[MQTT] Config loaded: %s:%d user='%s'\n", s_mqtt_host, s_mqtt_port, s_mqtt_user);
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
    console.printf("[MQTT] Config saved: %s:%d user='%s'\n", host, port, user);
    return true;
}

bool mqtt_client_connect() {
    if (!is_valid_host()) return false;

    s_mqtt.setServer(s_mqtt_host, s_mqtt_port);
    s_mqtt.setCallback(mqtt_callback);

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "gateway_%06x", chip_id());

    bool ok = false;
    if (strlen(s_mqtt_user) > 0) {
        ok = s_mqtt.connect(client_id, s_mqtt_user, s_mqtt_pass);
    } else {
        ok = s_mqtt.connect(client_id);
    }

    if (ok) {
        s_mqtt_connected = true;
        s_mqtt_connected_since = millis();
        s_should_reconnect = false;
        log_add("info", "MQTT conectado a %s:%d", s_mqtt_host, s_mqtt_port);
        console.printf("[MQTT] Connected to %s:%d\n", s_mqtt_host, s_mqtt_port);

        s_mqtt.subscribe("homeassistant/switch/+/set");
        s_mqtt.subscribe("homeassistant/light/+/set");

        mqtt_client_publish_all();
    } else {
        log_add("error", "MQTT falhou: rc=%d", s_mqtt.state());
        console.printf("[MQTT] Connection failed rc=%d\n", s_mqtt.state());
    }

    return ok;
}

void mqtt_client_disconnect() {
    s_mqtt.disconnect();
    s_mqtt_connected = false;
    s_mqtt_connected_since = 0;
    log_add("warn", "MQTT desconectado");
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
unsigned long mqtt_client_connected_since() { return s_mqtt_connected_since; }
const char* mqtt_client_get_host() { return s_mqtt_host; }
uint16_t mqtt_client_get_port() { return s_mqtt_port; }
const char* mqtt_client_get_user() { return s_mqtt_user; }
const char* mqtt_client_get_pass() { return s_mqtt_pass; }

const char* get_gateway_device_id() {
    static char id[48];
    uint32_t cid = chip_id();
    snprintf(id, sizeof(id), PLATFORM_PREFIX "_gateway_%06x", cid);
    return id;
}

void mqtt_client_generate_device_ids() {
    uint32_t cid = chip_id();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) == 0) {
            snprintf(s->bridge_device_id, sizeof(s->bridge_device_id),
                     "gw_%02X%02X%02X_%d", s->mac[3], s->mac[4], s->mac[5], i);
        }
    }
}

bool mqtt_client_publish_discovery(virtual_sensor_t *sensor) {
    if (!s_mqtt_connected) return false;

    const char *id = sensor->bridge_device_id;
    const char *name = sensor->name;
    const char *model = sensor_type_to_string(sensor->type);
    char entity[32];

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
            publish_entity_config("sensor", entity, name, "Temperatura", "temperature", "°C", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.temp_hum.temperature, 1).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
            publish_entity_config("sensor", entity, name, "Umidade", "humidity", "%", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.temp_hum.humidity, 0).c_str());
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "cnt");
            publish_entity_config("binary_sensor", entity, name, "Contato", "door", "", true, id, model);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.contact.contact_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_MOTION: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "occ");
            publish_entity_config("binary_sensor", entity, name, "Movimento", "occupancy", "", true, id, model);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.motion.motion_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_GAS: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "gas");
            publish_entity_config("sensor", entity, name, "Gás", "gas", "ppm", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.gas.gas_level).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "alm");
            publish_entity_config("binary_sensor", entity, name, "Alarme", "smoke", "", true, id, model);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_DHT_GAS: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
            publish_entity_config("sensor", entity, name, "Temperatura", "temperature", "°C", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.temperature, 1).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
            publish_entity_config("sensor", entity, name, "Umidade", "humidity", "%", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.humidity, 0).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "gas");
            publish_entity_config("sensor", entity, name, "Gás", "gas", "ppm", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.dht_gas.gas_level).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "alm");
            publish_entity_config("binary_sensor", entity, name, "Alarme", "smoke", "", true, id, model);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.dht_gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_RAIN: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "rain");
            publish_entity_config("sensor", entity, name, "Chuva", "moisture", "%", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.rain.rain_level).c_str());

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "raind");
            publish_entity_config("binary_sensor", entity, name, "Chuva Digital", "moisture", "", true, id, model);
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.rain.rain_digital ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_SOIL_MOISTURE: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "soil");
            publish_entity_config("sensor", entity, name, "Humidade Solo", "humidity", "%", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.soil_moisture.moisture_pct).c_str());
            break;
        }
        case SENSOR_TYPE_TANK: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lvl");
            publish_entity_config("sensor", entity, name, "Nível", "water", "%", false, id, model);
            publish_entity_state("sensor", entity,
                                 String(sensor->state.tank.level_pct).c_str());
            break;
        }
        case SENSOR_TYPE_ONOFF: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "pwr");
            publish_entity_config("switch", entity, name, "Interruptor", "", "", false, id, model);
            publish_entity_state("switch", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_LIGHT: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lgt");
            publish_entity_config("light", entity, name, "Lâmpada", "", "", false, id, model);
            publish_entity_state("light", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
    }

    build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "bat");
    publish_entity_config("sensor", entity, name, "Bateria", "battery", "%", false, id, model);
    publish_entity_state("sensor", entity, String(sensor->battery_pct).c_str());

    return true;
}

bool mqtt_client_publish_state(virtual_sensor_t *sensor) {
    if (!s_mqtt_connected) return false;

    char entity[32];

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
            snprintf(val, sizeof(val), "%.1f", sensor->state.temp_hum.temperature);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
            snprintf(val, sizeof(val), "%.0f", sensor->state.temp_hum.humidity);
            publish_entity_state("sensor", entity, val);
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "cnt");
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.contact.contact_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_MOTION: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "occ");
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.motion.motion_state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_GAS: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "gas");
            snprintf(val, sizeof(val), "%u", sensor->state.gas.gas_level);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "alm");
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_DHT_GAS: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
            snprintf(val, sizeof(val), "%.1f", sensor->state.dht_gas.temperature);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
            snprintf(val, sizeof(val), "%.0f", sensor->state.dht_gas.humidity);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "gas");
            snprintf(val, sizeof(val), "%u", sensor->state.dht_gas.gas_level);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "alm");
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.dht_gas.alarm ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_RAIN: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "rain");
            snprintf(val, sizeof(val), "%u", sensor->state.rain.rain_level);
            publish_entity_state("sensor", entity, val);

            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "raind");
            publish_entity_state("binary_sensor", entity,
                                 sensor->state.rain.rain_digital ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_SOIL_MOISTURE: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "soil");
            snprintf(val, sizeof(val), "%u", sensor->state.soil_moisture.moisture_pct);
            publish_entity_state("sensor", entity, val);
            break;
        }
        case SENSOR_TYPE_TANK: {
            char val[16];
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lvl");
            snprintf(val, sizeof(val), "%u", sensor->state.tank.level_pct);
            publish_entity_state("sensor", entity, val);
            break;
        }
        case SENSOR_TYPE_ONOFF: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "pwr");
            publish_entity_state("switch", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
        case SENSOR_TYPE_LIGHT: {
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lgt");
            publish_entity_state("light", entity,
                                 sensor->state.onoff.state ? "ON" : "OFF");
            break;
        }
    }

    build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "bat");
    publish_entity_state("sensor", entity, String(sensor->battery_pct).c_str());

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
    console.printf("[MQTT] Published %d sensors to MQTT\n", count);
    return count > 0;
}
