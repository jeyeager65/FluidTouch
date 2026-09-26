// Desktop implementation of Arduino IPAddress (IPv4 only).
#pragma once

#include <cstdint>
#include "WString.h"

class IPAddress {
public:
    IPAddress() : addr_{0, 0, 0, 0} {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : addr_{a, b, c, d} {}
    // Network byte order, same as ESP32 (first octet in lowest byte)
    IPAddress(uint32_t v)
        : addr_{uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)} {}

    operator uint32_t() const {
        return uint32_t(addr_[0]) | uint32_t(addr_[1]) << 8 | uint32_t(addr_[2]) << 16 | uint32_t(addr_[3]) << 24;
    }
    bool operator==(const IPAddress &o) const { return uint32_t(*this) == uint32_t(o); }
    bool operator!=(const IPAddress &o) const { return !(*this == o); }
    uint8_t operator[](int i) const { return addr_[i]; }
    uint8_t &operator[](int i) { return addr_[i]; }

    bool fromString(const char *s);
    bool fromString(const String &s) { return fromString(s.c_str()); }
    String toString() const;

private:
    uint8_t addr_[4];
};

#define INADDR_NONE_IP IPAddress(0, 0, 0, 0)
