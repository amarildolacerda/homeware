#include "common_util.h"

void uptime_to_str(unsigned long ms, char *buf, size_t len) {
    unsigned long s = ms / 1000;
    unsigned long d = s / 86400;
    unsigned long h = (s % 86400) / 3600;
    unsigned long m = (s % 3600) / 60;
    unsigned long sec = s % 60;
    if (d > 0) snprintf(buf, len, "%lud %luh%lum%lus", d, h, m, sec);
    else if (h > 0) snprintf(buf, len, "%luh%lum%lus", h, m, sec);
    else if (m > 0) snprintf(buf, len, "%lum%lus", m, sec);
    else snprintf(buf, len, "%lus", sec);
}

void uptime_to_str(unsigned long ms, String &out) {
    char buf[32];
    uptime_to_str(ms, buf, sizeof(buf));
    out = buf;
}
