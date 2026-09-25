// Desktop stub for the Arduino Wire (I2C) library.
#pragma once

#include <Arduino.h>

class TwoWire {
public:
    bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) { return true; }
    void end() {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission(bool = true) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    void setClock(uint32_t) {}
};

extern TwoWire Wire;
