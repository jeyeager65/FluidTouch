// Desktop WiFiClient: a plain TCP socket (Winsock / BSD sockets).
// Semantics follow the ESP32 core: non-blocking available()/read().
#pragma once

#include <Arduino.h>
#include <memory>

class WiFiClient : public Stream {
public:
    WiFiClient();
    explicit WiFiClient(int socket);
    ~WiFiClient() override;
    WiFiClient(const WiFiClient &) = default;
    WiFiClient &operator=(const WiFiClient &) = default;

    int connect(IPAddress ip, uint16_t port) { return connect(ip.toString().c_str(), port); }
    int connect(const char *host, uint16_t port);
    int connect(const char *host, uint16_t port, int32_t timeout_ms);
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int available() override;
    int read() override;
    int read(uint8_t *buf, size_t size);
    int peek() override;
    void flush() override {}
    void stop();
    uint8_t connected();
    int setNoDelay(bool nodelay);
    void setTimeout(uint32_t seconds) { Stream::setTimeout(seconds * 1000); }
    explicit operator bool() { return connected(); }

private:
    struct Socket;
    std::shared_ptr<Socket> sock_;  // shared like the ESP32 core's WiFiClientSocketHandle
};
