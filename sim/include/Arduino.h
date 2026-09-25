// Desktop implementation of the subset of the Arduino-ESP32 core that
// FluidTouch uses. Implemented in sim/src/arduino_core.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "WString.h"
#include "Print.h"
#include "IPAddress.h"

typedef uint8_t byte;
typedef bool boolean;

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05

#define PROGMEM
#define IRAM_ATTR
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))

// ESP32 core exposes std::min/max/abs rather than macros
using std::abs;
using std::max;
using std::min;

template <typename T, typename L, typename H>
inline T constrain(T v, L lo, H hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Timing - millis() is time since process start
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void yield();

// Random
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

// GPIO / peripherals - no-ops on desktop
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return LOW; }
inline uint16_t analogRead(uint8_t) { return 0; }
inline bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
inline bool ledcWrite(uint8_t, uint32_t) { return true; }
inline void btStop() {}
inline void gpio_deep_sleep_hold_en() {}

// POSIX time helper missing from MinGW's default headers
#ifdef _WIN32
inline struct tm *localtime_r(const time_t *t, struct tm *out) {
    return localtime_s(out, t) == 0 ? out : nullptr;
}
#endif

// Serial - prints to stdout, reads lines typed into the console
class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    void end() {}
    int available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    void flush() override;
    explicit operator bool() const { return true; }
};
extern HardwareSerial Serial;

// ESP chip helpers
class EspClass {
public:
    [[noreturn]] void restart();
    uint32_t getFreeHeap() { return 256 * 1024; }
    uint32_t getHeapSize() { return 512 * 1024; }
    uint32_t getMinFreeHeap() { return 200 * 1024; }
    uint32_t getMaxAllocHeap() { return 128 * 1024; }
    uint32_t getPsramSize() { return 8 * 1024 * 1024; }
    uint32_t getFreePsram() { return 6 * 1024 * 1024; }
    const char *getChipModel() { return "ESP32-S3 (simulator)"; }
    uint8_t getChipRevision() { return 0; }
    uint8_t getChipCores() { return 2; }
    uint32_t getCpuFreqMHz() { return 240; }
    uint32_t getFlashChipSize() { return 16 * 1024 * 1024; }
    const char *getSdkVersion() { return "sim"; }
    uint64_t getEfuseMac() { return 0x0000DEADBEEF0001ULL; }
};
extern EspClass ESP;

// Simulator hooks (set by sim_main.cpp)
namespace sim {
// Called periodically from delay() so the SDL window stays responsive while
// firmware code blocks (e.g. the WiFi connect loop).
extern void (*delay_hook)();
// Directory holding simulated NVS (preferences.json) and SD card (sd/)
const char *dataDir();
}
