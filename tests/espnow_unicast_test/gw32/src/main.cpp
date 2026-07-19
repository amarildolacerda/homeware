#include <WiFi.h>
#include <esp_now.h>

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
// ESP32 Arduino >= 2.x: recv_cb recebe esp_now_recv_info_t*
#else
// older: (uint8_t*, uint8_t*, uint8_t)
#endif

static unsigned long s_rx_unicast = 0;
static unsigned long s_rx_broadcast = 0;
static unsigned long s_last_print = 0;

#define WIFI_SSID "kcasa"
#define WIFI_PASS "3938373635"

// ESP32 Arduino (core antigo) callback signature: (mac, data, len)
void on_recv(const uint8_t *mac, const uint8_t *data, int len) {
    bool is_bcast = (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF &&
                     mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF);
    if (is_bcast) s_rx_broadcast++; else s_rx_unicast++;

    Serial.printf("[RX] %s from " MACSTR " len=%d\n",
                   is_bcast ? "BROADCAST" : "UNICAST",
                   MAC2STR(mac), len);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 GW TEST (unicast vs broadcast) ===");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando em "); Serial.print(WIFI_SSID);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        delay(500); Serial.print("."); tries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi OK IP=%s ch=%d\n", WiFi.localIP().toString().c_str(), WiFi.channel());
    } else {
        Serial.println("WiFi FALHOU (continua sem AP)");
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("esp_now_init FAILED");
        return;
    }
    esp_now_register_recv_cb(on_recv);

    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.printf("GW MAC (use no client UNICAST): " MACSTR "\n", MAC2STR(mac));
    Serial.printf("Modo: STA %s (com AP).\n", WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado");
}

void loop() {
    unsigned long now = millis();
    if (now - s_last_print > 5000) {
        s_last_print = now;
        Serial.printf("[STATS] unicast=%lu broadcast=%lu\n", s_rx_unicast, s_rx_broadcast);
    }
    delay(10);
}
