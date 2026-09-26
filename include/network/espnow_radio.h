#ifndef ESPNOW_RADIO_H
#define ESPNOW_RADIO_H

#include <stddef.h>
#include <stdint.h>

constexpr size_t ESPNOW_MAX_PAYLOAD = 250;

struct EspNowFrame {
    uint8_t src[6];
    uint8_t channel;  // WiFi channel the frame arrived on
    int8_t rssi;
    uint8_t len;
    uint8_t data[ESPNOW_MAX_PAYLOAD];
};

// Frame-level ESP-NOW access. The ESP32 implementation wraps esp_now; the
// desktop simulator's sends frames over UDP to sim/tools/fake_fluidnc.py.
class EspNowRadio {
public:
    virtual ~EspNowRadio() = default;

    virtual bool begin(const uint8_t pmk[16]) = 0;
    virtual void localMac(uint8_t mac[6]) = 0;
    virtual bool setChannel(uint8_t channel) = 0;
    // Adds or updates a peer on the current channel; lmk == nullptr means unencrypted
    virtual bool setPeer(const uint8_t mac[6], const uint8_t* lmk) = 0;
    virtual void removePeer(const uint8_t mac[6]) = 0;
    virtual bool send(const uint8_t mac[6], const void* data, size_t len) = 0;
    // Non-blocking; false when no frame is waiting
    virtual bool receive(EspNowFrame& frame) = 0;
};

// Provided by the platform (ESP32 firmware or simulator)
EspNowRadio& espnowPlatformRadio();

#endif  // ESPNOW_RADIO_H
