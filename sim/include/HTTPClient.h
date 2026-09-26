// Minimal desktop HTTPClient (plain http://, GET only) built on WiFiClient.
#pragma once

#include <Arduino.h>
#include "WiFiClient.h"

#define HTTPC_ERROR_CONNECTION_REFUSED (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED (-2)
#define HTTPC_ERROR_NOT_CONNECTED (-4)
#define HTTPC_ERROR_CONNECTION_LOST (-5)
#define HTTPC_ERROR_READ_TIMEOUT (-11)

#define HTTP_CODE_OK 200

class HTTPClient {
public:
    bool begin(const String &url);
    bool begin(WiFiClient &, const String &url) { return begin(url); }
    void end() { client_.stop(); }
    void setTimeout(uint16_t ms) { timeout_ms_ = ms; }
    void setConnectTimeout(int32_t ms) { timeout_ms_ = ms; }
    int GET();
    String getString() { return body_; }
    int getSize() { return (int)body_.length(); }
    static String errorToString(int error);

private:
    WiFiClient client_;
    String host_;
    String path_ = "/";
    uint16_t port_ = 80;
    uint32_t timeout_ms_ = 5000;
    String body_;
};
