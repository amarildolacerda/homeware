#pragma once
#include "display_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

class MockDisplay : public DisplayInterface {
public:
    bool m_begin_ret = true;
    bool m_begin_called = false;
    char m_last_text[128] = "";
    int m_cursor_x = 0, m_cursor_y = 0;
    int m_text_size = 1;
    int m_clear_count = 0;
    int m_display_count = 0;

    bool begin() override { m_begin_called = true; return m_begin_ret; }
    void clear() override { m_clear_count++; m_last_text[0] = '\0'; }
    void set_cursor(int x, int y) override { m_cursor_x = x; m_cursor_y = y; }
    void set_text_size(int size) override { m_text_size = size; }
    void print(const char* str) override {
        strncpy(m_last_text, str, sizeof(m_last_text) - 1);
    }
    void printf(const char* fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        vsnprintf(m_last_text, sizeof(m_last_text), fmt, args);
        va_end(args);
    }
    void display() override { m_display_count++; }
    int width() const override { return 128; }
    int height() const override { return 64; }
};
