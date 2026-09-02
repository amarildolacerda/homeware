#include "telegram_bot.h"
#include "config.h"
#include "config_store.h"
#include "sensor_registry.h"
#include "device_router.h"
#include "mqtt_client.h"
#include "common_console.h"
#include "platform.h"
#include <ctype.h>
#include <strings.h>

#if defined(TELEGRAM_ENABLED)

#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

// Telegram bot state
static bool s_tg_enabled = false;
static bool s_tg_initialized = false;
static unsigned long s_tg_last_poll = 0;
static uint32_t s_tg_poll_interval = 2000;
static char s_tg_token[64];
static char s_tg_chatid[20];
static uint8_t s_tg_alerts_lvl = 0x0F;
static uint16_t s_tg_alerts_type = 0x03FF;

// Alert throttling (per-type timestamps)
static unsigned long s_tg_throttle_gas = 0;
static unsigned long s_tg_throttle_smoke = 0;
static unsigned long s_tg_throttle_offline = 0;
static unsigned long s_tg_throttle_reconnect = 0;
static unsigned long s_tg_throttle_battery = 0;
static unsigned long s_tg_throttle_temperature = 0;
static unsigned long s_tg_throttle_humidity = 0;
static unsigned long s_tg_throttle_rssi = 0;
static unsigned long s_tg_throttle_heap = 0;
static unsigned long s_tg_throttle_daily = 0;

// Throttle intervals (ms)
#define THROTTLE_CRITICAL_MS    0       // No throttle for critical
#define THROTTLE_ALERT_MS       300000  // 5 min
#define THROTTLE_WARNING_MS     3600000 // 1 hour
#define THROTTLE_INFO_MS        300000  // 5 min
#define THROTTLE_GAS_MS         60000   // 1 min
#define THROTTLE_OFFLINE_MS     300000  // 5 min
#define THROTTLE_BATTERY_MS     3600000 // 1 hour
#define THROTTLE_DAILY_MS       86400000 // 24 hours

// WiFi client and bot instance
static WiFiClientSecure s_tg_client;
static UniversalTelegramBot* s_tg_bot = nullptr;

// Authorized users (from EEPROM or hardcoded)
static long s_tg_authorized_users[4] = {0};
static int s_tg_num_authorized = 0;

// Load configuration from LittleFS
static void telegram_load_config() {
    TelegramConfig cfg;
    config_telegram_load(&cfg);
    
    s_tg_enabled = cfg.enabled;
    strncpy(s_tg_token, cfg.token, sizeof(s_tg_token) - 1);
    s_tg_token[sizeof(s_tg_token) - 1] = '\0';
    strncpy(s_tg_chatid, cfg.chat_id, sizeof(s_tg_chatid) - 1);
    s_tg_chatid[sizeof(s_tg_chatid) - 1] = '\0';
    s_tg_poll_interval = cfg.poll_interval;
    s_tg_alerts_lvl = cfg.alerts_level;
    s_tg_alerts_type = cfg.alerts_type;
    
    // Parse chat ID as authorized user
    if (strlen(s_tg_chatid) > 0) {
        s_tg_authorized_users[0] = atol(s_tg_chatid);
        s_tg_num_authorized = 1;
    }
    
    console.printf("[TELEGRAM] Config loaded: enabled=%d, chat_id=%s, poll=%ums\n", 
                   s_tg_enabled, s_tg_chatid, s_tg_poll_interval);
}

// Check if user is authorized
static bool is_authorized(long chat_id) {
    for (int i = 0; i < s_tg_num_authorized; i++) {
        if (s_tg_authorized_users[i] == chat_id) return true;
    }
    return false;
}

// Check if alert type is enabled
static bool is_alert_type_enabled(int type_bit) {
    return (s_tg_alerts_type & (1 << type_bit)) != 0;
}

// Check if alert level is enabled
static bool is_alert_level_enabled(int level_bit) {
    return (s_tg_alerts_lvl & (1 << level_bit)) != 0;
}

// Check throttle
static bool check_throttle(unsigned long &last_send, unsigned long throttle_ms) {
    if (throttle_ms == 0) return true; // No throttle
    if (millis() - last_send >= throttle_ms) {
        last_send = millis();
        return true;
    }
    return false;
}

// Format uptime string
static void format_uptime(unsigned long ms, char* buf, size_t len) {
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    unsigned long hr = min / 60;
    unsigned long day = hr / 24;
    
    if (day > 0) {
        snprintf(buf, len, "%lud %luh %lum", day, hr % 24, min % 60);
    } else if (hr > 0) {
        snprintf(buf, len, "%luh %lum", hr, min % 60);
    } else {
        snprintf(buf, len, "%lum %lus", min, sec % 60);
    }
}

// Handle /status command
static void handle_status(long chat_id) {
    char buf[512];
    char uptime_str[32];
    format_uptime(millis(), uptime_str, sizeof(uptime_str));
    
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    
    snprintf(buf, sizeof(buf),
        "📊 *Status do Hub*\n\n"
        "Device: `%s`\n"
        "IP: `%s`\n"
        "WiFi: %s (%d dBm)\n"
        "Uptime: %s\n"
        "Memória: %u bytes (%u%%)\n\n"
        "Sensores: %d pareados, %d online\n"
        "MQTT: %s\n"
        "Telegram: %s",
        get_gateway_device_id(),
        WiFi.localIP().toString().c_str(),
        WiFi.SSID().c_str(),
        WiFi.RSSI(),
        uptime_str,
        ESP.getFreeHeap(),
        (ESP.getFreeHeap() * 100) / ESP.getHeapSize(),
        paired,
        online,
        mqtt_client_is_connected() ? "Conectado" : "Desconectado",
        s_tg_enabled ? "Ativo" : "Inativo"
    );
    
    s_tg_bot->sendMessage(String(chat_id), buf, "Markdown");
}

// Handle /list command
static void handle_list(long chat_id) {
    char buf[1024];
    int pos = 0;
    int count = 0;
    
    pos += snprintf(buf + pos, sizeof(buf) - pos, "📋 *Nodes Pareados*\n\n");
    
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired) {
            const char* status = s->online ? "🟢" : "🔴";
            const char* radio = "";
            switch (s->radio_type) {
                case RADIO_ESPNOW: radio = "ESP-NOW"; break;
                case RADIO_LORA: radio = "LoRa"; break;
                case RADIO_TCP: radio = "TCP"; break;
                default: radio = "?"; break;
            }
            
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "%s [%d] %s (%s) - %s\n",
                status, s->slot, s->name, radio,
                s->online ? sensor_type_friendly_name(s->type) : "offline"
            );
            count++;
            
            if (pos > 900) { // Prevent buffer overflow
                pos += snprintf(buf + pos, sizeof(buf) - pos, "\n... e mais nodes\n");
                break;
            }
        }
    }
    
    if (count == 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "Nenhum sensor pareado.\n");
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\nTotal: %d nodes", count);
    }
    
    s_tg_bot->sendMessage(String(chat_id), buf, "Markdown");
}

// Helper: resolve slot by numeric id or case-insensitive name
static int find_slot_by_arg(const char* args) {
    if (!args || strlen(args) == 0) return -1;
    // numeric slot?
    bool is_num = true;
    for (const char* p = args; *p; p++) if (!isdigit((unsigned char)*p)) { is_num = false; break; }
    if (is_num) {
        int slot = atoi(args);
        if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return -1;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired) return slot;
        return -1;
    }
    // case-insensitive name exact match
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (!s || !s->paired) continue;
        if (strcasecmp(s->name, args) == 0) return i;
    }
    return -1;
}

// Handle /on command
static void handle_on(long chat_id, const char* args) {
    if (!args || strlen(args) == 0) {
        s_tg_bot->sendMessage(String(chat_id), "Uso: /on <slot>", "");
        return;
    }
    
    int slot = find_slot_by_arg(args);
    if (slot < 0) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Node não encontrado: " + String(args), "");
        return;
    }
    
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || !s->paired) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Node não encontrado", "");
        return;
    }
    
    if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Tipo de sensor não suporta comando ON/OFF", "");
        return;
    }
    
    if (device_send_command(s->mac, s->slot, 1)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "✅ %s ligado!", s->name);
        s_tg_bot->sendMessage(String(chat_id), buf, "");
        console.printf("[TELEGRAM] ON command sent to %s\n", s->name);
    } else {
        s_tg_bot->sendMessage(String(chat_id), "❌ Falha ao enviar comando", "");
    }
}

// Handle /off command
static void handle_off(long chat_id, const char* args) {
    if (!args || strlen(args) == 0) {
        s_tg_bot->sendMessage(String(chat_id), "Uso: /off <slot>", "");
        return;
    }
    
    int slot = find_slot_by_arg(args);
    if (slot < 0) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Node não encontrado: " + String(args), "");
        return;
    }
    
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || !s->paired) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Node não encontrado", "");
        return;
    }
    
    if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Tipo de sensor não suporta comando ON/OFF", "");
        return;
    }
    
    if (device_send_command(s->mac, s->slot, 0)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "❌ %s desligado!", s->name);
        s_tg_bot->sendMessage(String(chat_id), buf, "");
        console.printf("[TELEGRAM] OFF command sent to %s\n", s->name);
    } else {
        s_tg_bot->sendMessage(String(chat_id), "❌ Falha ao enviar comando", "");
    }
}

// Handle /battery command
static void handle_battery(long chat_id) {
    char buf[512];
    int pos = 0;
    int count = 0;
    
    pos += snprintf(buf + pos, sizeof(buf) - pos, "🔋 *Níveis de Bateria*\n\n");
    
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired) {
            const char* icon = "✅";
            if (s->battery_pct < 20) icon = "🔴";
            else if (s->battery_pct < 50) icon = "🟡";
            
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "%s %s: %d%%\n",
                icon, s->name, s->battery_pct
            );
            count++;
            
            if (pos > 900) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "\n... e mais nodes\n");
                break;
            }
        }
    }
    
    if (count == 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "Nenhum sensor com bateria.\n");
    }
    
    s_tg_bot->sendMessage(String(chat_id), buf, "Markdown");
}

static String buildLampKeyboard() {
    String kb = "[";
    bool first = true;
    int cnt = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (!s || !s->paired) continue;
        if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) continue;
        if (!first) kb += ",";
        kb += "[\"toggle_" + String(s->slot) + " " + String(s->name) + " [" + (s->state.onoff.state ? "ON" : "OFF") + "]\"]";
        first = false;
        if (++cnt >= 6) break;
    }
    kb += "]";
    return kb;
}

// Handle toggle via reply keyboard (command, feedback via node)
static void handle_toggle(long chat_id, int slot) {
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || !s->paired) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Slot não encontrado", "");
        return;
    }
    if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) {
        s_tg_bot->sendMessage(String(chat_id), "❌ Tipo não suporta toggle", "");
        return;
    }
    uint8_t new_state = s->state.onoff.state ? 0 : 1;
    if (device_send_command(s->mac, s->slot, new_state)) {
        // feedback via node state report -> telegram_on_lamp_state_change()
        console.printf("[TELEGRAM] Toggle cmd slot %d -> %d (aguardando feedback)\n", slot, new_state);
    } else {
        s_tg_bot->sendMessage(String(chat_id), "❌ Falha ao enviar toggle", "");
    }
}

void telegram_on_lamp_state_change(int slot) {
    if (!s_tg_initialized || !s_tg_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;
    static unsigned long last_kb = 0;
    if (millis() - last_kb < 2000) return; // throttle 2s
    last_kb = millis();
    virtual_sensor_t *s = sensor_registry_get(slot);
    if (!s || (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT)) return;
    String kb = buildLampKeyboard();
    char buf[128];
    snprintf(buf, sizeof(buf), "🔄 %s → %s", s->name, s->state.onoff.state ? "ON" : "OFF");
    s_tg_bot->sendMessageWithReplyKeyboard(String(s_tg_chatid), buf, "", kb, true, false, false);
}

// Handle /start and /help commands
static void handle_help(long chat_id) {
    const char* help = 
        "🌱 *Bem-vindo ao AgriSense!*\n\n"
        "*Comandos disponíveis:*\n"
        "/start - Mensagem de boas-vindas\n"
        "/status - Status geral do hub\n"
        "/list - Listar todos os nodes [slot]\n"
        "/on <slot> - Liga relé\n"
        "/off <slot> - Desliga relé\n"
        "/battery - Níveis de bateria\n"
        "/help - Esta mensagem\n\n";
       // "*Exemplos:*\n"
       // "`/on 0`\n"
       // "`/off 0`\n"
       // "`/on Entrada` (case-insensitive)\n\n"
       // "Configure alertas via dashboard: http://<hub-ip>/settings";
    
    String keyboard = buildLampKeyboard();
    bool has_lamps = (keyboard != "[]");
    if (has_lamps) {
        s_tg_bot->sendMessageWithReplyKeyboard(String(chat_id), help, "Markdown", keyboard, true, false, false);
    } else {
        s_tg_bot->sendMessage(String(chat_id), help, "Markdown");
    }
}

// Process incoming messages
static void process_messages(int num_new_messages) {
    for (int i = 0; i < num_new_messages; i++) {
        String chat_id = String(s_tg_bot->messages[i].chat_id);
        long chat_id_long = chat_id.toInt();
        String text = s_tg_bot->messages[i].text;
        String from_name = s_tg_bot->messages[i].from_name;
        
        console.printf("[TELEGRAM] Message from %s: %s\n", from_name.c_str(), text.c_str());
        
        // Check authorization
        if (!is_authorized(chat_id_long)) {
            console.printf("[TELEGRAM] Unauthorized access from %s\n", chat_id.c_str());
            s_tg_bot->sendMessage(String(chat_id_long), 
                "❌ Acesso não autorizado.\n"
                "Seu Chat ID: " + chat_id + "\n"
                "Adicione este ID na whitelist do hub.", "");
            continue;
        }
        
        // Inline button callback (toggle_<slot>) - comes as callback_query type
        if (text.startsWith("toggle_") || s_tg_bot->messages[i].type == "callback_query") {
            String data = text;
            if (data.startsWith("toggle_")) {
                int slot = data.substring(7).toInt();
                handle_toggle(chat_id_long, slot);
                // Answer callback to dismiss loading spinner
                if (s_tg_bot->messages[i].query_id.length() > 0) {
                    s_tg_bot->answerCallbackQuery(s_tg_bot->messages[i].query_id, "");
                }
                continue;
            }
        }
        // Parse command and args
        String cmd = text;
        String args = "";
        int space_idx = text.indexOf(' ');
        if (space_idx > 0) {
            cmd = text.substring(0, space_idx);
            args = text.substring(space_idx + 1);
            args.trim();
        }
        cmd.toLowerCase();
        
        // Handle commands
        if (cmd == "/start" || cmd == "/help") {
            handle_help(chat_id_long);
        }
        else if (cmd == "/status") {
            handle_status(chat_id_long);
        }
        else if (cmd == "/list") {
            handle_list(chat_id_long);
        }
        else if (cmd == "/on") {
            handle_on(chat_id_long, args.c_str());
        }
        else if (cmd == "/off") {
            handle_off(chat_id_long, args.c_str());
        }
        else if (cmd == "/battery") {
            handle_battery(chat_id_long);
        }
        else {
            s_tg_bot->sendMessage(String(chat_id_long), 
                "❌ Comando desconhecido: " + cmd + "\nEnvie /help para ver os comandos.", "");
        }
    }
}

// Initialize Telegram bot
void telegram_bot_init() {
    telegram_load_config();
    
    if (!s_tg_enabled || strlen(s_tg_token) == 0) {
        console.println("[TELEGRAM] Bot disabled or no token configured");
        return;
    }
    
    // Validate token format (must contain ':' and be longer than 20 chars)
    if (strlen(s_tg_token) < 20 || strchr(s_tg_token, ':') == nullptr) {
        console.printf("[TELEGRAM] Invalid token format (len=%d), skipping init\n", strlen(s_tg_token));
        s_tg_enabled = false;
        return;
    }
    
    // Validate chat_id (must be numeric)
    if (strlen(s_tg_chatid) == 0 || atol(s_tg_chatid) == 0) {
        console.printf("[TELEGRAM] Invalid chat_id: '%s', skipping init\n", s_tg_chatid);
        s_tg_enabled = false;
        return;
    }
    
    console.println("[TELEGRAM] Initializing bot...");
    
    // Configure SSL
    s_tg_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    
    // Create bot instance
    s_tg_bot = new UniversalTelegramBot(s_tg_token, s_tg_client);
    s_tg_bot->longPoll = 0; // Disable long poll for responsiveness
    
    s_tg_initialized = true;
    s_tg_last_poll = millis();
    
    console.printf("[TELEGRAM] Bot initialized, polling every %ums\n", s_tg_poll_interval);
    
    // Send startup welcome (same as /start with toggle buttons)
    handle_help(atol(s_tg_chatid));
}

// Main loop
void telegram_bot_loop() {
    if (!s_tg_initialized || !s_tg_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Check polling interval
    if (millis() - s_tg_last_poll < s_tg_poll_interval) return;
    s_tg_last_poll = millis();
    
    // Get updates
    int num_new_messages = s_tg_bot->getUpdates(s_tg_bot->last_message_received + 1);
    
    if (num_new_messages > 0) {
        console.printf("[TELEGRAM] Received %d messages\n", num_new_messages);
        process_messages(num_new_messages);
    }
}

// Send alert notification
void telegram_send_alert(const char* level, const char* message) {
    if (!s_tg_initialized || !s_tg_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (strlen(s_tg_chatid) == 0) return;
    
    long chat_id = atol(s_tg_chatid);
    
    // Format message with level prefix
    char buf[512];
    snprintf(buf, sizeof(buf), "%s %s", level, message);
    
    s_tg_bot->sendMessage(String(chat_id), buf, "");
    console.printf("[TELEGRAM] Alert sent: %s\n", level);
}

// Check if Telegram is enabled
bool telegram_is_enabled() {
    return s_tg_enabled && s_tg_initialized;
}

#else // TELEGRAM_ENABLED not defined

// Stub implementations when Telegram is disabled
void telegram_bot_init() {}
void telegram_bot_loop() {}
void telegram_send_alert(const char* level, const char* message) {}
bool telegram_is_enabled() { return false; }

#endif // TELEGRAM_ENABLED
