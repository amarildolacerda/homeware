#include <ESP8266WiFi.h>
#include <espnow.h>

// Preencha com o MAC do ESP32 (impresso no serial do gw32).
// Ex: uint8_t GW_MAC[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
uint8_t GW_MAC[6] = {0xb4, 0xe6, 0x2d, 0xea, 0x4b, 0x41};

#define WIFI_SSID "kcasa"
#define WIFI_PASS "3938373635"

static uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_use_unicast = false;
static unsigned long s_last_send = 0;

void on_send(uint8_t *mac, uint8_t status) {
    Serial.printf("[TX CB] status=%d (0=OK) to " MACSTR "\n", status, MAC2STR(mac));
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP8266 CLIENT TEST (unicast vs broadcast) ===");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando em "); Serial.print(WIFI_SSID);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        delay(500); Serial.print("."); tries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi OK ch=%d\n", WiFi.channel());
    } else {
        Serial.println("WiFi FALHOU (continua sem AP)");
    }

    if (esp_now_init() != 0) {
        Serial.println("esp_now_init FAILED");
        return;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(on_send);

    // adiciona peer unicast (ESP32) no mesmo canal
    int ch = WiFi.channel();
    esp_now_add_peer(GW_MAC, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    esp_now_add_peer(BCAST, ESP_NOW_ROLE_COMBO, ch, NULL, 0);

    uint8_t self_mac2[6];
    WiFi.macAddress(self_mac2);
    Serial.printf("Client MAC: " MACSTR "\n", MAC2STR(self_mac2));
    Serial.println("Alternando UNICAST <-> BROADCAST a cada 5s.");
}

void loop() {
    unsigned long now = millis();
    if (now - s_last_send < 5000) return;
    s_last_send = now;

    uint8_t buf[8];
    uint8_t self_mac[6];
    WiFi.macAddress(self_mac);
    buf[0] = s_use_unicast ? 1 : 0;  // 1=unicast, 0=broadcast
    memcpy(buf + 1, self_mac, 6);

    int ret;
    if (s_use_unicast) {
        ret = esp_now_send(GW_MAC, buf, sizeof(buf));
        Serial.printf("[TX] UNICAST -> " MACSTR " ret=%d\n", MAC2STR(GW_MAC), ret);
    } else {
        ret = esp_now_send(BCAST, buf, sizeof(buf));
        Serial.printf("[TX] BROADCAST ret=%d\n", ret);
    }

    s_use_unicast = !s_use_unicast;  // troca modo
}
