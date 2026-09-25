// Desktop mDNS: .local names are resolved by the OS resolver (Windows 10+,
// macOS and Linux with nss-mdns all resolve .local via getaddrinfo).
#pragma once

#include <Arduino.h>

class MDNSResponder {
public:
    bool begin(const char *hostName) { return true; }
    void end() {}
    // hostName WITHOUT ".local", as on ESP32
    IPAddress queryHost(const char *hostName, uint32_t timeout = 2000);
    IPAddress queryHost(const String &hostName, uint32_t timeout = 2000) {
        return queryHost(hostName.c_str(), timeout);
    }
    bool addService(const char *, const char *, uint16_t) { return true; }
};

extern MDNSResponder MDNS;
