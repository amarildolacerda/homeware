#include "log_buffer.h"
#include "config.h"
#include <stdarg.h>

typedef struct {
    unsigned long time;
    char msg[LOG_MSG_MAX];
    char level[8];
} log_entry_t;

static log_entry_t s_buffer[LOG_BUFFER_SIZE];
static int s_head = 0;
static int s_count = 0;
static char s_json[4096];

void log_buffer_init() {
    s_head = 0;
    s_count = 0;
    log_add("info", "Gateway iniciado v" FW_VERSION);
}

void log_add(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_buffer[s_head].msg, LOG_MSG_MAX, fmt, args);
    va_end(args);
    s_buffer[s_head].time = millis();
    strncpy(s_buffer[s_head].level, level, sizeof(s_buffer[s_head].level) - 1);
    s_buffer[s_head].level[sizeof(s_buffer[s_head].level) - 1] = '\0';
    s_head = (s_head + 1) % LOG_BUFFER_SIZE;
    if (s_count < LOG_BUFFER_SIZE) s_count++;
}

const char* log_get_json() {
    int pos = 0;
    pos += snprintf(s_json + pos, sizeof(s_json) - pos, "{\"logs\":[");
    int start = (s_count < LOG_BUFFER_SIZE) ? 0 : s_head;
    int count = s_count;
    bool first = true;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        if (!first) {
            if (pos + 1 >= (int)sizeof(s_json)) break;
            s_json[pos++] = ',';
        }
        first = false;
        char esc[LOG_MSG_MAX * 2 + 1];
        int ei = 0;
        for (int si = 0; s_buffer[idx].msg[si] && ei < (int)sizeof(esc) - 2; si++) {
            char c = s_buffer[idx].msg[si];
            if (c == '"' || c == '\\') { esc[ei++] = '\\'; if (ei >= (int)sizeof(esc) - 2) break; }
            esc[ei++] = c;
        }
        esc[ei] = '\0';
        pos += snprintf(s_json + pos, sizeof(s_json) - pos,
            "{\"t\":%lu,\"msg\":\"%s\",\"level\":\"%s\"}",
            s_buffer[idx].time, esc, s_buffer[idx].level);
        if (pos >= (int)sizeof(s_json)) break;
    }
    pos += snprintf(s_json + pos, sizeof(s_json) - pos, "]}");
    if (pos < (int)sizeof(s_json)) s_json[pos] = '\0';
    return s_json;
}
