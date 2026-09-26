// Desktop networking for the simulator: WiFi/mDNS stubs that use the host's
// network, a real TCP WiFiClient, a minimal HTTPClient, and the TCP client
// used by ArduinoWebsockets (see sim/include/sim_ws_platform.h).

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#ifdef _WIN32
// Arduino.h (force-included) defines INPUT/OUTPUT, which clash with windows.h
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
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close ::close
#endif

WiFiClass WiFi;
MDNSResponder MDNS;

namespace {

const int CONNECT_TIMEOUT_MS = 3000;
const int READ_TIMEOUT_MS = 5000;

void netInit() {
#ifdef _WIN32
    static bool done = false;
    if (!done) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        done = true;
    }
#endif
}

bool resolveIPv4(const char *host, sockaddr_in &out) {
    netInit();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return false;
    out = *reinterpret_cast<sockaddr_in *>(res->ai_addr);
    freeaddrinfo(res);
    return true;
}

void setNonBlocking(sock_t s, bool nb) {
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, nb ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

// Returns a connected (blocking-mode) socket or SOCK_INVALID
sock_t tcpConnect(const char *host, uint16_t port, int timeout_ms) {
    sockaddr_in addr{};
    if (!resolveIPv4(host, addr)) {
        printf("[SIM] net: could not resolve '%s'\n", host);
        return SOCK_INVALID;
    }
    addr.sin_port = htons(port);

    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCK_INVALID) return SOCK_INVALID;

    // Non-blocking connect so an unreachable machine times out quickly
    setNonBlocking(s, true);
    ::connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_SET(s, &wfds);
    FD_ZERO(&efds); FD_SET(s, &efds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int r = select((int)s + 1, nullptr, &wfds, &efds, &tv);
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
    if (r <= 0 || FD_ISSET(s, &efds) || err != 0) {
        sock_close(s);
        return SOCK_INVALID;
    }
    setNonBlocking(s, false);
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&one), sizeof(one));
    return s;
}

// Wait up to timeout_ms for the socket to become readable. 1 = readable
// (data or EOF), 0 = timeout, -1 = error.
int waitReadable(sock_t s, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    return select((int)s + 1, &rfds, nullptr, nullptr, &tv);
}

// Bytes ready to read without blocking; -1 if the peer closed/errored
int bytesAvailable(sock_t s) {
    int r = waitReadable(s, 0);
    if (r < 0) return -1;
    if (r == 0) return 0;
    char c;
    int n = recv(s, &c, 1, MSG_PEEK);
    if (n <= 0) return -1;  // readable with no data = closed
#ifdef _WIN32
    u_long count = 0;
    ioctlsocket(s, FIONREAD, &count);
#else
    int count = 0;
    ioctl(s, FIONREAD, &count);
#endif
    return count > 0 ? (int)count : 1;
}

bool sendAll(sock_t s, const uint8_t *data, size_t len) {
    while (len > 0) {
        int n = send(s, reinterpret_cast<const char *>(data), (int)std::min<size_t>(len, 1 << 20), 0);
        if (n <= 0) return false;
        data += n;
        len -= n;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// WiFi / mDNS
// ---------------------------------------------------------------------------
wl_status_t WiFiClass::begin(const char *ssid, const char *) {
    netInit();
    ssid_ = ssid ? ssid : "";
    connected_ = true;
    printf("[SIM] WiFi.begin('%s') - using the PC's network connection\n", ssid_.c_str());
    return WL_CONNECTED;
}

bool WiFiClass::disconnect(bool, bool) {
    connected_ = false;
    return true;
}

IPAddress WiFiClass::localIP() {
    // Find the LAN address the OS would use for outbound traffic
    netInit();
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_INVALID) return IPAddress(127, 0, 0, 1);
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    remote.sin_addr.s_addr = htonl(0x08080808);  // no packet is sent for UDP connect
    IPAddress ip(127, 0, 0, 1);
    if (::connect(s, reinterpret_cast<sockaddr *>(&remote), sizeof(remote)) == 0) {
        sockaddr_in local{};
        socklen_t len = sizeof(local);
        if (getsockname(s, reinterpret_cast<sockaddr *>(&local), &len) == 0) {
            ip = IPAddress((uint32_t)local.sin_addr.s_addr);
        }
    }
    sock_close(s);
    return ip;
}

int WiFiClass::hostByName(const char *host, IPAddress &result) {
    sockaddr_in addr{};
    if (!resolveIPv4(host, addr)) {
        result = IPAddress();
        return 0;
    }
    result = IPAddress((uint32_t)addr.sin_addr.s_addr);
    return 1;
}

IPAddress MDNSResponder::queryHost(const char *hostName, uint32_t) {
    std::string fqdn = std::string(hostName) + ".local";
    IPAddress ip;
    WiFi.hostByName(fqdn.c_str(), ip);
    return ip;
}

// ---------------------------------------------------------------------------
// WiFiClient
// ---------------------------------------------------------------------------
struct WiFiClient::Socket {
    sock_t fd = SOCK_INVALID;
    explicit Socket(sock_t s) : fd(s) {}
    ~Socket() { if (fd != SOCK_INVALID) sock_close(fd); }
};

WiFiClient::WiFiClient() = default;
WiFiClient::WiFiClient(int socket) : sock_(std::make_shared<Socket>((sock_t)socket)) {}
WiFiClient::~WiFiClient() = default;

int WiFiClient::connect(const char *host, uint16_t port) {
    return connect(host, port, CONNECT_TIMEOUT_MS);
}

int WiFiClient::connect(const char *host, uint16_t port, int32_t timeout_ms) {
    stop();
    sock_t s = tcpConnect(host, port, timeout_ms);
    if (s == SOCK_INVALID) return 0;
    sock_ = std::make_shared<Socket>(s);
    return 1;
}

size_t WiFiClient::write(const uint8_t *buf, size_t size) {
    if (!sock_ || !sendAll(sock_->fd, buf, size)) return 0;
    return size;
}

int WiFiClient::available() {
    if (!sock_) return 0;
    int n = bytesAvailable(sock_->fd);
    return n < 0 ? 0 : n;
}

int WiFiClient::read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
}

int WiFiClient::read(uint8_t *buf, size_t size) {
    if (!sock_ || available() <= 0) return -1;
    int n = recv(sock_->fd, reinterpret_cast<char *>(buf), (int)size, 0);
    return n <= 0 ? -1 : n;
}

int WiFiClient::peek() {
    if (!sock_ || available() <= 0) return -1;
    char c;
    return recv(sock_->fd, &c, 1, MSG_PEEK) == 1 ? (uint8_t)c : -1;
}

void WiFiClient::stop() { sock_.reset(); }

uint8_t WiFiClient::connected() {
    return sock_ && bytesAvailable(sock_->fd) >= 0;
}

int WiFiClient::setNoDelay(bool nodelay) {
    if (!sock_) return -1;
    int v = nodelay ? 1 : 0;
    return setsockopt(sock_->fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&v), sizeof(v));
}

// ---------------------------------------------------------------------------
// HTTPClient (GET only - used by UploadManager to create directories)
// ---------------------------------------------------------------------------
bool HTTPClient::begin(const String &url) {
    String u = url;
    if (!u.startsWith("http://")) return false;
    u = u.substring(7);
    int slash = u.indexOf('/');
    String hostport = slash >= 0 ? u.substring(0, slash) : u;
    path_ = slash >= 0 ? u.substring(slash) : String("/");
    int colon = hostport.indexOf(':');
    host_ = colon >= 0 ? hostport.substring(0, colon) : hostport;
    port_ = colon >= 0 ? (uint16_t)hostport.substring(colon + 1).toInt() : 80;
    return true;
}

int HTTPClient::GET() {
    body_ = "";
    if (!client_.connect(host_.c_str(), port_, (int32_t)timeout_ms_)) return HTTPC_ERROR_CONNECTION_REFUSED;
    client_.print("GET " + path_ + " HTTP/1.1\r\nHost: " + host_ + "\r\nConnection: close\r\n\r\n");

    // Read the whole response (server closes the connection)
    std::string resp;
    unsigned long start = millis();
    while (millis() - start < timeout_ms_) {
        uint8_t buf[1024];
        int n = client_.read(buf, sizeof(buf));
        if (n > 0) {
            resp.append(reinterpret_cast<char *>(buf), n);
            start = millis();
        } else if (!client_.connected()) {
            break;
        } else {
            delay(5);
        }
    }
    client_.stop();
    if (resp.empty()) return HTTPC_ERROR_READ_TIMEOUT;

    int code = 0;
    sscanf(resp.c_str(), "HTTP/%*s %d", &code);
    size_t bodyStart = resp.find("\r\n\r\n");
    if (bodyStart != std::string::npos) body_ = String(resp.substr(bodyStart + 4));
    return code > 0 ? code : HTTPC_ERROR_CONNECTION_LOST;
}

String HTTPClient::errorToString(int error) {
    switch (error) {
        case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
        case HTTPC_ERROR_SEND_HEADER_FAILED: return "send header failed";
        case HTTPC_ERROR_NOT_CONNECTED: return "not connected";
        case HTTPC_ERROR_CONNECTION_LOST: return "connection lost";
        case HTTPC_ERROR_READ_TIMEOUT: return "read Timeout";
        default: return "unknown error";
    }
}

// ---------------------------------------------------------------------------
// ArduinoWebsockets TCP client
// ---------------------------------------------------------------------------
namespace websockets { namespace network {

SimTcpClient::SimTcpClient() : sock_((intptr_t)SOCK_INVALID) {}
SimTcpClient::~SimTcpClient() { close(); }

bool SimTcpClient::connect(const WSString &host, int port) {
    close();
    sock_ = (intptr_t)tcpConnect(host.c_str(), (uint16_t)port, CONNECT_TIMEOUT_MS);
    return (sock_t)sock_ != SOCK_INVALID;
}

bool SimTcpClient::poll() {
    if ((sock_t)sock_ == SOCK_INVALID) return false;
    return bytesAvailable((sock_t)sock_) > 0;
}

bool SimTcpClient::available() {
    if ((sock_t)sock_ == SOCK_INVALID) return false;
    if (bytesAvailable((sock_t)sock_) < 0) {
        close();
        return false;
    }
    return true;
}

void SimTcpClient::send(const WSString &data) { send(reinterpret_cast<const uint8_t *>(data.data()), (uint32_t)data.size()); }
void SimTcpClient::send(const WSString &&data) { send(reinterpret_cast<const uint8_t *>(data.data()), (uint32_t)data.size()); }

void SimTcpClient::send(const uint8_t *data, const uint32_t len) {
    if ((sock_t)sock_ == SOCK_INVALID) return;
    if (!sendAll((sock_t)sock_, data, len)) close();
}

WSString SimTcpClient::readLine() {
    WSString line;
    uint8_t ch = 0;
    while (ch != '\n') {
        if (read(&ch, 1) != 1) return "";
        line += (char)ch;
    }
    return line;
}

uint32_t SimTcpClient::read(uint8_t *buffer, const uint32_t len) {
    uint32_t got = 0;
    while (got < len) {
        if ((sock_t)sock_ == SOCK_INVALID) return static_cast<uint32_t>(-1);
        int w = waitReadable((sock_t)sock_, READ_TIMEOUT_MS);
        if (w <= 0) {
            close();
            return static_cast<uint32_t>(-1);
        }
        int n = recv((sock_t)sock_, reinterpret_cast<char *>(buffer + got), (int)(len - got), 0);
        if (n <= 0) {
            close();
            return static_cast<uint32_t>(-1);
        }
        got += n;
    }
    return got;
}

void SimTcpClient::close() {
    if ((sock_t)sock_ != SOCK_INVALID) {
        sock_close((sock_t)sock_);
        sock_ = (intptr_t)SOCK_INVALID;
    }
}

int SimTcpClient::getSocket() const { return (int)sock_; }

}} // namespace websockets::network
