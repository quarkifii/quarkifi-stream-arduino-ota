// Host-side mock of the Arduino core — just enough for QuarkifiStreamOTA.
// Provides: byte, String, millis/delay/yield (virtual clock), ESP (restart/reset).
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <string>

typedef uint8_t byte;

#define HEX 16
#define DEC 10

// ---------------------------------------------------------------- String
class String {
public:
    String() {}
    String(const char *s) : _s(s ? s : "") {}
    String(const std::string &s) : _s(s) {}
    String(char c) : _s(1, c) {}
    String(int v, unsigned char base = DEC) : _s(toBase((long)v, base)) {}
    String(unsigned int v, unsigned char base = DEC) : _s(toBase((long)v, base)) {}
    String(long v, unsigned char base = DEC) : _s(toBase(v, base)) {}
    String(unsigned long v, unsigned char base = DEC) : _s(toBase((long)v, base)) {}

    const char *c_str() const { return _s.c_str(); }
    unsigned int length() const { return (unsigned int)_s.size(); }
    bool isEmpty() const { return _s.empty(); }
    bool startsWith(const String &p) const { return _s.compare(0, p._s.size(), p._s) == 0; }
    bool endsWith(const String &p) const {
        return _s.size() >= p._s.size() &&
               _s.compare(_s.size() - p._s.size(), p._s.size(), p._s) == 0;
    }
    int indexOf(char c) const {
        size_t i = _s.find(c);
        return i == std::string::npos ? -1 : (int)i;
    }
    bool reserve(unsigned int n) { _s.reserve(n); return true; }
    char operator[](unsigned int i) const { return _s[i]; }

    String &operator+=(const String &o) { _s += o._s; return *this; }
    String &operator+=(const char *o) { _s += o; return *this; }
    String &operator+=(char c) { _s += c; return *this; }

    friend String operator+(const String &a, const String &b) { return String(a._s + b._s); }
    friend String operator+(const String &a, const char *b) { return String(a._s + b); }
    friend String operator+(const char *a, const String &b) { return String(std::string(a) + b._s); }

    bool operator==(const String &o) const { return _s == o._s; }
    bool operator==(const char *o) const { return _s == o; }
    bool operator!=(const String &o) const { return _s != o._s; }

    const std::string &str() const { return _s; }

private:
    static std::string toBase(long v, unsigned char base) {
        if (base == DEC) return std::to_string(v);
        static const char *digits = "0123456789abcdef";
        if (v == 0) return "0";
        unsigned long u = (unsigned long)v;
        std::string r;
        while (u) { r.insert(r.begin(), digits[u % base]); u /= base; }
        return r;
    }
    std::string _s;
};

// ---------------------------------------------------------------- Time
// Virtual clock: millis() only advances via delay() (or explicitly by mocks).
unsigned long millis();
void delay(unsigned long ms);
void yield();

// ---------------------------------------------------------------- ESP
class EspClass {
public:
    void restart();
    void reset();
};
extern EspClass ESP;
