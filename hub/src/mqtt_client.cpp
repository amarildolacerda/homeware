#include "mqtt_client.h"
#include "config.h"
#include "config_store.h"
#include "sensor_registry.h"
#include "platform.h"
#define MQTT_MAX_PACKET_SIZE 768
#include "log_buffer.h"
#include "common_console.h"
#include "device_router.h"
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
static unsigned long s_reconnect_interval_ms = 30000;
static unsigned int s_consecutive_fails = 0;

#define MQTT_TOPIC_PREFIX "homeassistant"
#define MQTT_RECONNECT_INITIAL_MS 30000
#define MQTT_RECONNECT_MAX_MS     300000
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
            device_send_command(s->mac, s->slot, state);
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

    {
        char avail_topic[128];
        snprintf(avail_topic, sizeof(avail_topic), "%s/%s/%s/availability",
                 MQTT_TOPIC_PREFIX, component, entity_id);
        doc["availability_topic"] = avail_topic;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
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

bool mqtt_client_load_config() {
    MQTTConfig cfg;
    if (config_mqtt_load(&cfg)) {
        strncpy(s_mqtt_host, cfg.host, sizeof(s_mqtt_host) - 1);
        s_mqtt_host[sizeof(s_mqtt_host) - 1] = '\0';
        s_mqtt_port = cfg.port;
        strncpy(s_mqtt_user, cfg.user, sizeof(s_mqtt_user) - 1);
        s_mqtt_user[sizeof(s_mqtt_user) - 1] = '\0';
        strncpy(s_mqtt_pass, cfg.password, sizeof(s_mqtt_pass) - 1);
        s_mqtt_pass[sizeof(s_mqtt_pass) - 1] = '\0';
    } else {
        strcpy(s_mqtt_host, MQTT_HOST_DEFAULT);
        s_mqtt_port = MQTT_PORT_DEFAULT;
        s_mqtt_user[0] = '\0';
        s_mqtt_pass[0] = '\0';
    }

    console.printf("[MQTT] Config loaded: %s:%d user='%s'\n", s_mqtt_host, s_mqtt_port, s_mqtt_user);
    return true;
}

bool mqtt_client_save_config(const char *host, uint16_t port, const char *user, const char *pass) {
    MQTTConfig cfg;
    strncpy(cfg.host, host, sizeof(cfg.host) - 1);
    cfg.host[sizeof(cfg.host) - 1] = '\0';
    cfg.port = port;
    strncpy(cfg.user, user, sizeof(cfg.user) - 1);
    cfg.user[sizeof(cfg.user) - 1] = '\0';
    strncpy(cfg.password, pass, sizeof(cfg.password) - 1);
    cfg.password[sizeof(cfg.password) - 1] = '\0';

    bool ok = config_mqtt_save(&cfg);

    strcpy(s_mqtt_host, host);
    s_mqtt_port = port;
    strcpy(s_mqtt_user, user);
    strcpy(s_mqtt_pass, pass);

    s_should_reconnect = true;
    s_reconnect_interval_ms = MQTT_RECONNECT_INITIAL_MS;
    s_consecutive_fails = 0;
    s_last_reconnect = 0;
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
        s_reconnect_interval_ms = MQTT_RECONNECT_INITIAL_MS;
        s_consecutive_fails = 0;
        log_add("info", "MQTT conectado a %s:%d", s_mqtt_host, s_mqtt_port);
        console.printf("[MQTT] Connected to %s:%d\n", s_mqtt_host, s_mqtt_port);

        s_mqtt.subscribe("homeassistant/switch/+/set");
        s_mqtt.subscribe("homeassistant/light/+/set");

        mqtt_client_publish_all();

        mqtt_client_publish_offline_all();
    } else {
        s_should_reconnect = false;
        s_consecutive_fails++;
        unsigned long new_interval = MQTT_RECONNECT_INITIAL_MS << min(s_consecutive_fails, 5u);
        if (new_interval > MQTT_RECONNECT_MAX_MS) new_interval = MQTT_RECONNECT_MAX_MS;
        s_reconnect_interval_ms = new_interval;

        static unsigned long last_fail_log = 0;
        if (millis() - last_fail_log > 60000) {
            last_fail_log = millis();
            log_add("error", "MQTT falhou: rc=%d (retry em %lus)", s_mqtt.state(), s_reconnect_interval_ms / 1000);
            console.printf("[MQTT] Connection failed rc=%d (next retry in %lus)\n", s_mqtt.state(), s_reconnect_interval_ms / 1000);
        }
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
        if (now - s_last_reconnect > s_reconnect_interval_ms) {
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
        case SENSOR_TYPE_LEVEL: {
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
        case SENSOR_TYPE_LEVEL: {
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

bool mqtt_client_publish_availability(virtual_sensor_t *sensor, bool online) {
    if (!s_mqtt_connected || !sensor || !sensor->paired) return false;

    const char *payload = online ? "online" : "offline";
    char entity[32];
    char topic[128];
    int count = 0;

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_CONTACT:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "cnt");
            snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_MOTION:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "occ");
            snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "gas");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "alm");
            snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            if (sensor->type == SENSOR_TYPE_DHT_GAS) {
                build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "temp");
                snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
                s_mqtt.publish(topic, payload, true); count++;
                build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "hum");
                snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
                s_mqtt.publish(topic, payload, true); count++;
            }
            break;
        case SENSOR_TYPE_RAIN:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "rain");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "raind");
            snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_SOIL_MOISTURE:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "soil");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_LEVEL:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lvl");
            snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_ONOFF:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "pwr");
            snprintf(topic, sizeof(topic), "%s/switch/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_LIGHT:
            build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "lgt");
            snprintf(topic, sizeof(topic), "%s/light/%s/availability", MQTT_TOPIC_PREFIX, entity);
            s_mqtt.publish(topic, payload, true); count++;
            break;
        case SENSOR_TYPE_REPEATER:
            return false;
    }

    build_entity_id(entity, sizeof(entity), sensor->mac, sensor->slot, "bat");
    snprintf(topic, sizeof(topic), "%s/sensor/%s/availability", MQTT_TOPIC_PREFIX, entity);
    s_mqtt.publish(topic, payload, true); count++;

    console.printf("[MQTT] Availability %s for %s (%d entities)\n",
                   payload, sensor->name, count);
    return count > 0;
}

void mqtt_client_publish_offline_all() {
    if (!s_mqtt_connected) return;
    int count = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) > 0) {
            mqtt_client_publish_availability(s, false);
            count++;
        }
    }
    console.printf("[MQTT] Published offline availability for %d sensors\n", count);
}
