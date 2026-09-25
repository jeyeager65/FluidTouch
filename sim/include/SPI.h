// Desktop stub for the Arduino SPI library.
#pragma once

#include <Arduino.h>

class SPIClass {
public:
    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) {}
    void end() {}
};

extern SPIClass SPI;
