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
        esp_now_register_send_cb(onSent);
        _started = true;
        return true;
    }

    void localMac(uint8_t mac[6]) override { esp_wifi_get_mac(WIFI_IF_STA, mac); }

    bool setChannel(uint8_t channel) override {
        esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            LOG_PRINTF("[ESP-NOW] Set channel %u failed: %s\n", channel, esp_err_to_name(err));
        }
        return err == ESP_OK;
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
        // Re-add rather than modify, so switching a peer between plain and
        // encrypted can't be left half-done
        if (esp_now_is_peer_exist(mac)) {
            esp_now_del_peer(mac);
        }
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK) {
            LOG_PRINTF("[ESP-NOW] Adding %s peer %02x:%02x:%02x:%02x:%02x:%02x failed: %s\n",
                       lmk ? "encrypted" : "plain", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                       esp_err_to_name(err));
        }
        return err == ESP_OK;
    }

    void removePeer(const uint8_t mac[6]) override {
        if (esp_now_is_peer_exist(mac)) {
            esp_now_del_peer(mac);
        }
    }

    bool send(const uint8_t mac[6], const void* data, size_t len) override {
        esp_err_t err = esp_now_send(mac, (const uint8_t*)data, len);
        if (err != ESP_OK && _sendErrors++ < 10) {
            LOG_PRINTF("[ESP-NOW] Send failed: %s\n", esp_err_to_name(err));
        }
        return err == ESP_OK;
    }

    bool receive(EspNowFrame& frame) override {
        return _queue && xQueueReceive(_queue, &frame, 0) == pdTRUE;
    }

    void deliveryCounts(uint32_t& delivered, uint32_t& failed) override {
        delivered = _delivered;
        failed = _failed;
    }

private:
    static QueueHandle_t _queue;
    static volatile uint32_t _delivered;
    static volatile uint32_t _failed;
    bool _started = false;
    uint32_t _sendErrors = 0;

    // Runs in the WiFi task. Broadcasts always report success, so this
    // mostly tells us whether FluidNC's radio acknowledged a unicast frame.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    static void onSent(const esp_now_send_info_t*, esp_now_send_status_t status) {
#else
    static void onSent(const uint8_t*, esp_now_send_status_t status) {
#endif
        if (status == ESP_NOW_SEND_SUCCESS) {
            _delivered = _delivered + 1;
        } else {
            _failed = _failed + 1;
        }
    }

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
volatile uint32_t Esp32EspNowRadio::_delivered = 0;
volatile uint32_t Esp32EspNowRadio::_failed = 0;

}  // namespace

EspNowRadio& espnowPlatformRadio() {
    static Esp32EspNowRadio radio;
    return radio;
}

#endif  // FLUIDTOUCH_SIM
