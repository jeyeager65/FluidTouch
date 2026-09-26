// Desktop implementation of the Arduino core pieces declared in
// sim/include/Arduino.h, WString.h, Print.h and IPAddress.h.

#include <Arduino.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// String
// ---------------------------------------------------------------------------
std::string String::fromUnsigned(unsigned long long v, unsigned char base) {
    if (base < 2 || base > 36) base = 10;
    if (v == 0) return "0";
    std::string out;
    while (v) {
        int d = int(v % base);
        out.insert(out.begin(), char(d < 10 ? '0' + d : 'a' + d - 10));
        v /= base;
    }
    return out;
}

std::string String::fromSigned(long long v, unsigned char base) {
    if (base == 10 && v < 0) return "-" + fromUnsigned(0ULL - (unsigned long long)v, base);
    // Arduino prints negative non-decimal values as two's complement
    return fromUnsigned(base == 10 ? (unsigned long long)v : (unsigned long)v, base);
}

std::string String::fromDouble(double v, unsigned int decimals) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, v);
    return buf;
}

bool String::equalsIgnoreCase(const String &o) const {
    if (s_.size() != o.s_.size()) return false;
    for (size_t i = 0; i < s_.size(); i++) {
        if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i])) return false;
    }
    return true;
}

void String::replace(const String &find, const String &repl) {
    if (find.s_.empty()) return;
    size_t pos = 0;
    while ((pos = s_.find(find.s_, pos)) != std::string::npos) {
        s_.replace(pos, find.s_.size(), repl.s_);
        pos += repl.s_.size();
    }
}

void String::toLowerCase() { for (auto &c : s_) c = (char)tolower((unsigned char)c); }
void String::toUpperCase() { for (auto &c : s_) c = (char)toupper((unsigned char)c); }

void String::trim() {
    size_t b = s_.find_first_not_of(" \t\r\n\f\v");
    if (b == std::string::npos) { s_.clear(); return; }
    size_t e = s_.find_last_not_of(" \t\r\n\f\v");
    s_ = s_.substr(b, e - b + 1);
}

void String::toCharArray(char *buf, unsigned int size, unsigned int index) const {
    if (!buf || size == 0) return;
    size_t n = 0;
    if (index < s_.size()) {
        n = std::min<size_t>(size - 1, s_.size() - index);
        memcpy(buf, s_.data() + index, n);
    }
    buf[n] = '\0';
}

// ---------------------------------------------------------------------------
// Print / Stream
// ---------------------------------------------------------------------------
size_t Print::vprintf(const char *fmt, va_list args) {
    char stackbuf[512];
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(stackbuf, sizeof(stackbuf), fmt, copy);
    va_end(copy);
    if (len < 0) return 0;
    if ((size_t)len < sizeof(stackbuf)) return write(reinterpret_cast<const uint8_t *>(stackbuf), len);
    std::string big(len + 1, '\0');
    vsnprintf(&big[0], big.size(), fmt, args);
    return write(reinterpret_cast<const uint8_t *>(big.data()), len);
}

size_t Print::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    size_t n = vprintf(fmt, args);
    va_end(args);
    return n;
}

int Stream::timedRead() {
    unsigned long start = millis();
    do {
        int c = read();
        if (c >= 0) return c;
        delay(1);
    } while (millis() - start < timeout_);
    return -1;
}

size_t Stream::readBytes(char *buffer, size_t length) {
    size_t count = 0;
    while (count < length) {
        int c = timedRead();
        if (c < 0) break;
        buffer[count++] = (char)c;
    }
    return count;
}

String Stream::readString() {
    String out;
    int c;
    while ((c = timedRead()) >= 0) out += (char)c;
    return out;
}

String Stream::readStringUntil(char terminator) {
    String out;
    int c;
    while ((c = timedRead()) >= 0 && c != terminator) out += (char)c;
    return out;
}

// ---------------------------------------------------------------------------
// Serial: stdout for output; a background thread collects console input so
// main.cpp's "type a command to send it to FluidNC" feature works.
// ---------------------------------------------------------------------------
namespace {
std::mutex g_stdin_mutex;
std::deque<char> g_stdin_buf;
std::once_flag g_stdin_once;

void startStdinReader() {
    std::call_once(g_stdin_once, [] {
        std::thread([] {
            std::string line;
            while (std::getline(std::cin, line)) {
                std::lock_guard<std::mutex> lock(g_stdin_mutex);
                for (char c : line) g_stdin_buf.push_back(c);
                g_stdin_buf.push_back('\n');
            }
        }).detach();
    });
}
} // namespace

HardwareSerial Serial;

int HardwareSerial::available() {
    startStdinReader();
    std::lock_guard<std::mutex> lock(g_stdin_mutex);
    return (int)g_stdin_buf.size();
}

int HardwareSerial::read() {
    std::lock_guard<std::mutex> lock(g_stdin_mutex);
    if (g_stdin_buf.empty()) return -1;
    char c = g_stdin_buf.front();
    g_stdin_buf.pop_front();
    return (unsigned char)c;
}

int HardwareSerial::peek() {
    std::lock_guard<std::mutex> lock(g_stdin_mutex);
    return g_stdin_buf.empty() ? -1 : (unsigned char)g_stdin_buf.front();
}

size_t HardwareSerial::write(uint8_t c) {
    // Firmware uses "\r\n" (println); drop the \r so console output is clean
    if (c != '\r') fputc(c, stdout);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buf[i] != '\r') fputc(buf[i], stdout);
    }
    return size;
}

void HardwareSerial::flush() { fflush(stdout); }

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
namespace {
const auto g_start = std::chrono::steady_clock::now();
}

unsigned long millis() {
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
}

unsigned long micros() {
    return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - g_start).count();
}

void (*sim::delay_hook)() = nullptr;

void delay(unsigned long ms) {
    // Sleep in small slices, pumping the window so long blocking waits in the
    // firmware (WiFi/mDNS retries) don't make the window "Not Responding".
    unsigned long start = millis();
    while (true) {
        if (sim::delay_hook) sim::delay_hook();
        unsigned long elapsed = millis() - start;
        if (elapsed >= ms) break;
        unsigned long slice = std::min<unsigned long>(ms - elapsed, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
    }
}

void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void yield() {}

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------
namespace {
std::mt19937 &rng() {
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}
} // namespace

long random(long max) { return max <= 0 ? 0 : long(rng()() % (unsigned long)max); }
long random(long min, long max) { return max <= min ? min : min + random(max - min); }
void randomSeed(unsigned long seed) { rng().seed(seed); }

// ---------------------------------------------------------------------------
// IPAddress
// ---------------------------------------------------------------------------
bool IPAddress::fromString(const char *s) {
    unsigned a, b, c, d;
    char extra;
    if (!s || sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    addr_[0] = a; addr_[1] = b; addr_[2] = c; addr_[3] = d;
    return true;
}

String IPAddress::toString() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", addr_[0], addr_[1], addr_[2], addr_[3]);
    return String(buf);
}

// ---------------------------------------------------------------------------
// ESP
// ---------------------------------------------------------------------------
EspClass ESP;

namespace sim {
extern int g_argc;
extern char **g_argv;
void shutdown();
}

void EspClass::restart() {
    // Emulate a reboot by relaunching the simulator with the same arguments.
    printf("\n[SIM] ESP.restart() - relaunching simulator\n\n");
    fflush(stdout);
    sim::shutdown();
#ifdef _WIN32
    // _spawnv joins arguments with spaces without quoting them
    std::vector<std::string> quoted;
    for (int i = 0; i < sim::g_argc; i++) {
        std::string a = sim::g_argv[i];
        quoted.push_back(a.find_first_of(" \t") != std::string::npos ? "\"" + a + "\"" : a);
    }
    std::vector<const char *> args;
    for (auto &q : quoted) args.push_back(q.c_str());
    args.push_back(nullptr);
    _spawnv(_P_NOWAIT, sim::g_argv[0], args.data());
    _exit(0);
#else
    execv(sim::g_argv[0], sim::g_argv);
    _exit(1);
#endif
}
