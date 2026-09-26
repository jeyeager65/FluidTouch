// Desktop WiFi: the simulator uses the PC's own network connection, so WiFi
// always reports "connected" once begin() is called. SSID/password from the
// machine config are logged and otherwise ignored.
#pragma once

#include <Arduino.h>
#include "WiFiClient.h"

typedef enum {
    WL_NO_SHIELD = 255,
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
} wl_status_t;

typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;

class WiFiClass {
public:
    wl_status_t begin(const char *ssid, const char *passphrase = nullptr);
    bool disconnect(bool wifioff = false, bool eraseap = false);
    bool mode(wifi_mode_t m) { mode_ = m; return true; }
    wifi_mode_t getMode() { return mode_; }
    bool setAutoReconnect(bool) { return true; }
    bool setSleep(bool) { return true; }
    bool setHostname(const char *) { return true; }
    wl_status_t status() { return connected_ ? WL_CONNECTED : WL_DISCONNECTED; }
    bool isConnected() { return connected_; }
    IPAddress localIP();
    String SSID() { return ssid_; }
    int8_t RSSI() { return connected_ ? -45 : 0; }
    String macAddress() { return "DE:AD:BE:EF:00:01"; }
    int hostByName(const char *host, IPAddress &result);

private:
    bool connected_ = false;
    wifi_mode_t mode_ = WIFI_OFF;
    String ssid_;
};

extern WiFiClass WiFi;
