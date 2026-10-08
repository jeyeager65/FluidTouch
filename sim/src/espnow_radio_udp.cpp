// Desktop ESP-NOW radio for the simulator: frames travel as UDP datagrams to
// the ESP-NOW side of sim/tools/fake_fluidnc.py.
//
// Datagram layout: channel, source MAC (6), destination MAC (6), encrypted
// flag, payload. The flag mirrors whether real hardware would encrypt the
// frame (the peer was added with a key), so the fake server can reject frames
// sent with the wrong encryption state.
//
// Server address: FT_ESPNOW_SERVER=host:port (default 127.0.0.1:8182).

#include <Arduino.h>

#include <map>
#include <string>

#include "network/espnow_radio.h"

#ifdef _WIN32
#undef INPUT
#undef OUTPUT
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close ::close
#endif

namespace {

constexpr size_t HEADER_SIZE = 14;
const uint8_t BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

class UdpEspNowRadio : public EspNowRadio {
public:
    bool begin(const uint8_t[16]) override {
        if (_sock != SOCK_INVALID) {
            return true;
        }
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        std::string host = "127.0.0.1";
        std::string port = "8182";
        if (const char* env = getenv("FT_ESPNOW_SERVER")) {
            std::string s = env;
            size_t colon = s.rfind(':');
            host = s.substr(0, colon);
            if (colon != std::string::npos) {
                port = s.substr(colon + 1);
            }
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
            Serial.printf("[ESP-NOW sim] Can't resolve %s:%s\n", host.c_str(), port.c_str());
            return false;
        }
        memcpy(&_server, res->ai_addr, sizeof(_server));
        freeaddrinfo(res);

        _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_sock == SOCK_INVALID) {
            return false;
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(_sock, (sockaddr*)&local, sizeof(local));
#ifdef _WIN32
        u_long nonblocking = 1;
        ioctlsocket(_sock, FIONBIO, &nonblocking);
#else
        fcntl(_sock, F_SETFL, fcntl(_sock, F_GETFL, 0) | O_NONBLOCK);
#endif
        Serial.printf("[ESP-NOW sim] Radio frames go to UDP %s:%s\n", host.c_str(), port.c_str());
        return true;
    }

    void localMac(uint8_t mac[6]) override {
        // Locally administered address, "FTSIM"
        const uint8_t sim[6] = {0x02, 0x46, 0x54, 0x53, 0x49, 0x4d};
        memcpy(mac, sim, 6);
    }

    bool setChannel(uint8_t channel) override {
        _channel = channel;
        return true;
    }

    bool setPeer(const uint8_t mac[6], const uint8_t* lmk) override {
        _peers[key(mac)] = lmk != nullptr;
        return true;
    }

    void removePeer(const uint8_t mac[6]) override { _peers.erase(key(mac)); }

    bool send(const uint8_t mac[6], const void* data, size_t len) override {
        auto peer = _peers.find(key(mac));
        if (_sock == SOCK_INVALID || peer == _peers.end() || len > ESPNOW_MAX_PAYLOAD) {
            return false;  // Like esp_now_send(), peers must be added first
        }
        uint8_t buf[HEADER_SIZE + ESPNOW_MAX_PAYLOAD];
        buf[0] = _channel;
        localMac(buf + 1);
        memcpy(buf + 7, mac, 6);
        buf[13] = peer->second ? 1 : 0;
        memcpy(buf + HEADER_SIZE, data, len);
        return sendto(_sock, (const char*)buf, (int)(HEADER_SIZE + len), 0, (sockaddr*)&_server, sizeof(_server)) >= 0;
    }

    bool receive(EspNowFrame& frame) override {
        if (_sock == SOCK_INVALID) {
            return false;
        }
        uint8_t buf[HEADER_SIZE + ESPNOW_MAX_PAYLOAD + 1];
        uint8_t mac[6];
        localMac(mac);
        for (;;) {
            int n = recvfrom(_sock, (char*)buf, sizeof(buf), 0, nullptr, nullptr);
            if (n < 0) {
                return false;
            }
            if (n < (int)HEADER_SIZE + 1 || n > (int)(HEADER_SIZE + ESPNOW_MAX_PAYLOAD)) {
                continue;
            }
            // The radio only hears frames on its current channel
            if (buf[0] != _channel || (memcmp(buf + 7, mac, 6) != 0 && memcmp(buf + 7, BROADCAST, 6) != 0)) {
                continue;
            }
            memcpy(frame.src, buf + 1, 6);
            frame.channel = buf[0];
            frame.rssi = -45;
            frame.len = (uint8_t)(n - HEADER_SIZE);
            memcpy(frame.data, buf + HEADER_SIZE, frame.len);
            return true;
        }
    }

private:
    sock_t _sock = SOCK_INVALID;
    sockaddr_in _server{};
    uint8_t _channel = 1;
    std::map<std::string, bool> _peers;  // MAC -> encrypted

    static std::string key(const uint8_t mac[6]) { return std::string((const char*)mac, 6); }
};

}  // namespace

EspNowRadio& espnowPlatformRadio() {
    static UdpEspNowRadio radio;
    return radio;
}
