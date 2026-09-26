// ESP32 ESP-NOW radio. The simulator's version is sim/src/espnow_radio_udp.cpp.
#ifndef FLUIDTOUCH_SIM

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#include "debug_log.h"
#include "network/espnow_radio.h"

namespace {

class Esp32EspNowRadio : public EspNowRadio {
public:
    bool begin(const uint8_t pmk[16]) override {
        if (_started) {
            return true;
        }
        // ESP-NOW needs the WiFi radio up, but not associated with an access
        // point: the channel has to follow FluidNC, not a router
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_ps(WIFI_PS_NONE);

        _queue = xQueueCreate(16, sizeof(EspNowFrame));
        if (!_queue) {
            return false;
        }
        esp_err_t err = esp_now_init();
        if (err != ESP_OK) {
            LOG_PRINTF("[ESP-NOW] esp_now_init failed: %s\n", esp_err_to_name(err));
            return false;
        }
        esp_now_set_pmk(pmk);
        esp_now_register_recv_cb(onReceive);
        _started = true;
        return true;
    }

    void localMac(uint8_t mac[6]) override { esp_wifi_get_mac(WIFI_IF_STA, mac); }

    bool setChannel(uint8_t channel) override {
        return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
    }

    bool setPeer(const uint8_t mac[6], const uint8_t* lmk) override {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = 0;  // Whatever channel the radio is on
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = lmk != nullptr;
        if (lmk) {
            memcpy(peer.lmk, lmk, 16);
        }
        if (esp_now_is_peer_exist(mac)) {
            return esp_now_mod_peer(&peer) == ESP_OK;
        }
        return esp_now_add_peer(&peer) == ESP_OK;
    }

    void removePeer(const uint8_t mac[6]) override {
        if (esp_now_is_peer_exist(mac)) {
            esp_now_del_peer(mac);
        }
    }

    bool send(const uint8_t mac[6], const void* data, size_t len) override {
        return esp_now_send(mac, (const uint8_t*)data, len) == ESP_OK;
    }

    bool receive(EspNowFrame& frame) override {
        return _queue && xQueueReceive(_queue, &frame, 0) == pdTRUE;
    }

private:
    static QueueHandle_t _queue;
    bool _started = false;

    // Runs in the WiFi task: copy the frame and hand it to the poll loop
    static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (!_queue || !info || len <= 0 || len > (int)ESPNOW_MAX_PAYLOAD) {
            return;
        }
        EspNowFrame frame;
        memcpy(frame.src, info->src_addr, 6);
        frame.channel = info->rx_ctrl ? info->rx_ctrl->channel : 0;
        frame.rssi = info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : -127;
        frame.len = (uint8_t)len;
        memcpy(frame.data, data, len);
        xQueueSend(_queue, &frame, 0);
    }
};

QueueHandle_t Esp32EspNowRadio::_queue = nullptr;

}  // namespace

EspNowRadio& espnowPlatformRadio() {
    static Esp32EspNowRadio radio;
    return radio;
}

#endif  // FLUIDTOUCH_SIM
