// Desktop implementation of Arduino Print / Stream base classes.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include "WString.h"

class Print;

class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print &p) const = 0;
};

class Print {
public:
    size_t print(const Printable &x) { return x.printTo(*this); }
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buf, size_t size) {
        size_t n = 0;
        while (size--) n += write(*buf++);
        return n;
    }
    size_t write(const char *s) { return s ? write(reinterpret_cast<const uint8_t *>(s), strlen(s)) : 0; }
    size_t write(const char *buf, size_t size) { return write(reinterpret_cast<const uint8_t *>(buf), size); }
    virtual void flush() {}

    size_t print(const String &s) { return write(s.c_str(), s.length()); }
    size_t print(const char *s) { return write(s); }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(unsigned char v, int base = DEC) { return print(String(v, base)); }
    size_t print(int v, int base = DEC) { return print(String(v, base)); }
    size_t print(unsigned int v, int base = DEC) { return print(String(v, base)); }
    size_t print(long v, int base = DEC) { return print(String(v, base)); }
    size_t print(unsigned long v, int base = DEC) { return print(String(v, base)); }
    size_t print(long long v, int base = DEC) { return print(String(v, base)); }
    size_t print(unsigned long long v, int base = DEC) { return print(String(v, base)); }
    size_t print(double v, int digits = 2) { return print(String(v, (unsigned)digits)); }

    size_t println() { return write("\r\n"); }
    template <typename T> size_t println(const T &v) { size_t n = print(v); return n + println(); }
    template <typename T> size_t println(const T &v, int fmt) { size_t n = print(v, fmt); return n + println(); }

    size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    size_t vprintf(const char *fmt, va_list args);
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    void setTimeout(unsigned long ms) { timeout_ = ms; }
    unsigned long getTimeout() const { return timeout_; }

    size_t readBytes(char *buffer, size_t length);
    size_t readBytes(uint8_t *buffer, size_t length) { return readBytes(reinterpret_cast<char *>(buffer), length); }
    String readString();
    String readStringUntil(char terminator);

protected:
    int timedRead();
    unsigned long timeout_ = 1000;
};
