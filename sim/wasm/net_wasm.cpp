// Browser networking for the simulator. There are no sockets in a page, so
// the TCP client ArduinoWebsockets uses (SimTcpClient, see
// sim/include/sim_ws_platform.h) is a byte pipe to the page: the page joins
// it to a controller running in the same page (FluidNC's wasm build:
// fluidnc_vconn_open/send/close into its own web server), so the WebSocket
// handshake and frames are the real ones. WiFi is always "connected";
// HTTP (uploads) isn't bridged yet.
//
// Page side (Module options):
//   ftNetOpen(port) -> id (0 = refused)      ftNetSend(id, Uint8Array)
//   ftNetClose(id)
// and it calls _ft_net_data(id, ptr, len) / _ft_net_closed(id) back.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <emscripten.h>

#include <deque>
#include <map>
#include <mutex>

WiFiClass WiFi;
MDNSResponder MDNS;

namespace {

const int READ_TIMEOUT_MS = 5000;

struct Conn {
    std::deque<uint8_t> rx;
    bool closed = false;
};
std::mutex g_mutex;
std::map<int, Conn> g_conns;

int pageOpen(int port) {
    return MAIN_THREAD_EM_ASM_INT({ return Module.ftNetOpen ? (Module.ftNetOpen($0) | 0) : 0; }, port);
}

void pageSend(int id, const uint8_t *data, uint32_t len) {
    MAIN_THREAD_EM_ASM({ if (Module.ftNetSend) Module.ftNetSend($0, HEAPU8.slice($1, $1 + $2)); }, id, data, len);
}

void pageClose(int id) {
    MAIN_THREAD_EM_ASM({ if (Module.ftNetClose) Module.ftNetClose($0); }, id);
}

} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void ft_net_data(int id, const uint8_t *data, int len) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_conns.find(id);
    if (it != g_conns.end()) it->second.rx.insert(it->second.rx.end(), data, data + len);
}

EMSCRIPTEN_KEEPALIVE void ft_net_closed(int id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_conns.find(id);
    if (it != g_conns.end()) it->second.closed = true;
}
}

// ---------------------------------------------------------------------------
// WiFi / mDNS
// ---------------------------------------------------------------------------
wl_status_t WiFiClass::begin(const char *ssid, const char *) {
    ssid_ = ssid ? ssid : "";
    connected_ = true;
    printf("[SIM] WiFi.begin('%s') - the page's virtual network\n", ssid_.c_str());
    return WL_CONNECTED;
}

bool WiFiClass::disconnect(bool, bool) {
    connected_ = false;
    return true;
}

IPAddress WiFiClass::localIP() { return IPAddress(192, 168, 0, 2); }

int WiFiClass::hostByName(const char *host, IPAddress &result) {
    // Only numeric addresses: the page answers whatever host is named
    if (result.fromString(host)) return 1;
    result = IPAddress();
    return 0;
}

IPAddress MDNSResponder::queryHost(const char *, uint32_t) { return IPAddress(); }

// ---------------------------------------------------------------------------
// WiFiClient / HTTPClient: not bridged (connect fails)
// ---------------------------------------------------------------------------
struct WiFiClient::Socket {};

WiFiClient::WiFiClient() = default;
WiFiClient::WiFiClient(int) {}
WiFiClient::~WiFiClient() = default;
int WiFiClient::connect(const char *host, uint16_t port) { return connect(host, port, 0); }
int WiFiClient::connect(const char *host, uint16_t port, int32_t) {
    printf("[SIM] WiFiClient: no plain TCP in the browser (%s:%u)\n", host, port);
    return 0;
}
size_t WiFiClient::write(const uint8_t *, size_t) { return 0; }
int WiFiClient::available() { return 0; }
int WiFiClient::read() { return -1; }
int WiFiClient::read(uint8_t *, size_t) { return -1; }
int WiFiClient::peek() { return -1; }
void WiFiClient::stop() { sock_.reset(); }
uint8_t WiFiClient::connected() { return 0; }
int WiFiClient::setNoDelay(bool) { return -1; }

bool HTTPClient::begin(const String &url) { return url.startsWith("http://"); }
int HTTPClient::GET() { return HTTPC_ERROR_CONNECTION_REFUSED; }
String HTTPClient::errorToString(int error) {
    return error == HTTPC_ERROR_CONNECTION_REFUSED ? "no HTTP in the browser build" : "unknown error";
}

// ---------------------------------------------------------------------------
// ArduinoWebsockets TCP client -> the page
// ---------------------------------------------------------------------------
namespace websockets { namespace network {

SimTcpClient::SimTcpClient() : sock_(0) {}
SimTcpClient::~SimTcpClient() { close(); }

bool SimTcpClient::connect(const WSString &host, int port) {
    close();
    int id = pageOpen(port);
    if (!id) {
        printf("[SIM] net: nothing listening at %s:%d in the page\n", host.c_str(), port);
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_conns[id];  // data that arrives before this is dropped: none can (the server speaks second)
    }
    sock_ = id;
    return true;
}

bool SimTcpClient::poll() {
    if (!sock_) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_conns.find((int)sock_);
    return it != g_conns.end() && !it->second.rx.empty();
}

bool SimTcpClient::available() {
    if (!sock_) return false;
    bool gone;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_conns.find((int)sock_);
        gone = it == g_conns.end() || (it->second.closed && it->second.rx.empty());
    }
    if (gone) close();
    return !gone;
}

void SimTcpClient::send(const WSString &data) { send(reinterpret_cast<const uint8_t *>(data.data()), (uint32_t)data.size()); }
void SimTcpClient::send(const WSString &&data) { send(reinterpret_cast<const uint8_t *>(data.data()), (uint32_t)data.size()); }

void SimTcpClient::send(const uint8_t *data, const uint32_t len) {
    if (!sock_ || !len) return;
    pageSend((int)sock_, data, len);
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
    unsigned long start = millis();
    while (got < len) {
        if (!sock_) return static_cast<uint32_t>(-1);
        bool closed = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_conns.find((int)sock_);
            if (it == g_conns.end()) {
                closed = true;
            } else {
                auto &rx = it->second.rx;
                while (got < len && !rx.empty()) {
                    buffer[got++] = rx.front();
                    rx.pop_front();
                }
                closed = it->second.closed && rx.empty() && got < len;
            }
        }
        if (got == len) break;
        if (closed || millis() - start > (unsigned long)READ_TIMEOUT_MS) {
            close();
            return static_cast<uint32_t>(-1);
        }
        delay(1);
    }
    return got;
}

void SimTcpClient::close() {
    if (!sock_) return;
    int id = (int)sock_;
    sock_ = 0;
    bool wasOpen;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_conns.find(id);
        wasOpen = it != g_conns.end() && !it->second.closed;
        g_conns.erase(id);
    }
    if (wasOpen) pageClose(id);
}

int SimTcpClient::getSocket() const { return (int)sock_; }

}} // namespace websockets::network
