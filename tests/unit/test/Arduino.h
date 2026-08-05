#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>

unsigned long millis();

// Minimal String mock for native tests
class String {
public:
    String() : _buf(nullptr), _len(0) {}
    String(const char* s) {
        if (s) {
            _len = strlen(s);
            _buf = new char[_len + 1];
            memcpy(_buf, s, _len + 1);
        } else {
            _buf = nullptr;
            _len = 0;
        }
    }
    String(const String& o) : _buf(nullptr), _len(0) { *this = o; }
    ~String() { delete[] _buf; }
    String& operator=(const String& o) {
        if (this != &o) {
            delete[] _buf;
            _len = o._len;
            if (o._buf) {
                _buf = new char[_len + 1];
                memcpy(_buf, o._buf, _len + 1);
            } else {
                _buf = nullptr;
            }
        }
        return *this;
    }
    const char* c_str() const { return _buf ? _buf : ""; }
    size_t length() const { return _len; }
    operator bool() const { return _len > 0; }
private:
    char* _buf;
    size_t _len;
};
