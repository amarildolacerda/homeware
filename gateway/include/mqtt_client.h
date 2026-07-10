#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include "config.h"
#include "sensor_registry.h"

bool mqtt_client_load_config();
bool mqtt_client_save_config(const char *host, uint16_t port, const char *user, const char *pass);
const char* mqtt_client_get_host();
uint16_t mqtt_client_get_port();
const char* mqtt_client_get_user();
const char* mqtt_client_get_pass();
bool mqtt_client_is_connected();
unsigned long mqtt_client_connected_since();
bool mqtt_client_connect();
void mqtt_client_disconnect();
void mqtt_client_loop();
bool mqtt_client_publish_discovery(virtual_sensor_t *sensor);
bool mqtt_client_publish_state(virtual_sensor_t *sensor);
bool mqtt_client_publish_all();
void mqtt_client_generate_device_ids();
const char* get_gateway_device_id();

#endif
