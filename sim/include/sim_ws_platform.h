// ArduinoWebsockets platform glue for the simulator.
//
// The library selects a TCP implementation per platform (ESP32, ESP8266,
// Teensy) in ws_common.hpp and has no desktop option, so this header is
// force-included (gcc -include) into every C++ file of the simulator build
// and supplies WSDefaultTcpClient / WSDefaultTcpServer backed by real
// desktop sockets (sim/src/net.cpp).
#pragma once

#ifdef __cplusplus

#include <tiny_websockets/network/tcp_client.hpp>
#include <tiny_websockets/network/tcp_server.hpp>

namespace websockets { namespace network {

// Blocking-read TCP client: read(buf, len) waits until exactly len bytes
// arrive (or the connection drops / times out and returns -1), which is what
// the library's frame parser expects. poll() is non-blocking, so the
// firmware's FluidNCClient::loop() -> webSocket.poll() never stalls the UI.
class SimTcpClient : public TcpClient {
public:
    SimTcpClient();
    ~SimTcpClient() override;
    bool connect(const WSString &host, int port) override;
    bool poll() override;
    bool available() override;
    void send(const WSString &data) override;
    void send(const WSString &&data) override;
    void send(const uint8_t *data, const uint32_t len) override;
    WSString readLine() override;
    uint32_t read(uint8_t *buffer, const uint32_t len) override;
    void close() override;

protected:
    int getSocket() const override;

private:
    intptr_t sock_;
};

// Server side is never used by FluidTouch; stub so server.hpp compiles.
class SimTcpServer : public TcpServer {
public:
    bool poll() override { return false; }
    bool listen(const uint16_t) override { return false; }
    TcpClient *accept() override { return new SimTcpClient; }
    bool available() override { return false; }
    void close() override {}

protected:
    int getSocket() const override { return -1; }
};

}} // namespace websockets::network

#define WSDefaultTcpClient websockets::network::SimTcpClient
#define WSDefaultTcpServer websockets::network::SimTcpServer

#endif // __cplusplus
