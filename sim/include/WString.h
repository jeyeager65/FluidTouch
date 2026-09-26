// Desktop implementation of the Arduino String class (subset used by
// FluidTouch, ArduinoJson and ArduinoWebsockets), backed by std::string.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef DEC
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
#endif

class __FlashStringHelper;

class String {
public:
    String() = default;
    String(const char *s) : s_(s ? s : "") {}
    String(const char *s, size_t len) : s_(s ? std::string(s, len) : std::string()) {}
    String(const std::string &s) : s_(s) {}
    String(const String &) = default;
    String(String &&) noexcept = default;
    String(const __FlashStringHelper *s) : String(reinterpret_cast<const char *>(s)) {}
    explicit String(char c) : s_(1, c) {}
    explicit String(unsigned char v, unsigned char base = 10) : s_(fromUnsigned(v, base)) {}
    explicit String(int v, unsigned char base = 10) : s_(fromSigned(v, base)) {}
    explicit String(unsigned int v, unsigned char base = 10) : s_(fromUnsigned(v, base)) {}
    explicit String(long v, unsigned char base = 10) : s_(fromSigned(v, base)) {}
    explicit String(unsigned long v, unsigned char base = 10) : s_(fromUnsigned(v, base)) {}
    explicit String(long long v, unsigned char base = 10) : s_(fromSigned(v, base)) {}
    explicit String(unsigned long long v, unsigned char base = 10) : s_(fromUnsigned(v, base)) {}
    explicit String(float v, unsigned int decimals = 2) : s_(fromDouble(v, decimals)) {}
    explicit String(double v, unsigned int decimals = 2) : s_(fromDouble(v, decimals)) {}

    String &operator=(const String &) = default;
    String &operator=(String &&) noexcept = default;
    String &operator=(const char *s) { s_ = s ? s : ""; return *this; }

    // Arduino String is always "valid" once constructed
    explicit operator bool() const { return true; }

    const char *c_str() const { return s_.c_str(); }
    unsigned int length() const { return (unsigned int)s_.length(); }
    bool isEmpty() const { return s_.empty(); }
    bool reserve(unsigned int size) { s_.reserve(size); return true; }
    const std::string &str() const { return s_; }

    char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
    void setCharAt(unsigned int i, char c) { if (i < s_.size()) s_[i] = c; }
    char operator[](unsigned int i) const { return charAt(i); }
    char &operator[](unsigned int i) { static char dummy; return i < s_.size() ? s_[i] : (dummy = 0); }

    // concat / append
    bool concat(const String &o) { s_ += o.s_; return true; }
    bool concat(const char *o) { if (o) s_ += o; return true; }
    bool concat(const char *o, unsigned int len) { if (o) s_.append(o, len); return true; }
    bool concat(char c) { s_ += c; return true; }
    bool concat(unsigned char v) { s_ += fromUnsigned(v, 10); return true; }
    bool concat(int v) { s_ += fromSigned(v, 10); return true; }
    bool concat(unsigned int v) { s_ += fromUnsigned(v, 10); return true; }
    bool concat(long v) { s_ += fromSigned(v, 10); return true; }
    bool concat(unsigned long v) { s_ += fromUnsigned(v, 10); return true; }
    bool concat(long long v) { s_ += fromSigned(v, 10); return true; }
    bool concat(unsigned long long v) { s_ += fromUnsigned(v, 10); return true; }
    bool concat(float v) { s_ += fromDouble(v, 2); return true; }
    bool concat(double v) { s_ += fromDouble(v, 2); return true; }

    template <typename T> String &operator+=(const T &v) { concat(v); return *this; }
    String &operator+=(const char *v) { concat(v); return *this; }

    // comparisons
    int compareTo(const String &o) const { return s_.compare(o.s_); }
    bool equals(const String &o) const { return s_ == o.s_; }
    bool equals(const char *o) const { return s_ == (o ? o : ""); }
    bool equalsIgnoreCase(const String &o) const;
    bool operator==(const String &o) const { return s_ == o.s_; }
    bool operator==(const char *o) const { return equals(o); }
    bool operator!=(const String &o) const { return s_ != o.s_; }
    bool operator!=(const char *o) const { return !equals(o); }
    bool operator<(const String &o) const { return s_ < o.s_; }
    bool operator>(const String &o) const { return s_ > o.s_; }
    bool operator<=(const String &o) const { return s_ <= o.s_; }
    bool operator>=(const String &o) const { return s_ >= o.s_; }
    bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0; }
    bool startsWith(const String &p, unsigned int offset) const {
        return offset <= s_.size() && s_.compare(offset, p.s_.size(), p.s_) == 0;
    }
    bool endsWith(const String &p) const {
        return p.s_.size() <= s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
    }

    // search
    int indexOf(char c, unsigned int from = 0) const { return npos(s_.find(c, from)); }
    int indexOf(const String &o, unsigned int from = 0) const { return npos(s_.find(o.s_, from)); }
    int lastIndexOf(char c) const { return npos(s_.rfind(c)); }
    int lastIndexOf(char c, unsigned int from) const { return npos(s_.rfind(c, from)); }
    int lastIndexOf(const String &o) const { return npos(s_.rfind(o.s_)); }
    int lastIndexOf(const String &o, unsigned int from) const { return npos(s_.rfind(o.s_, from)); }
    String substring(unsigned int from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
    String substring(unsigned int from, unsigned int to) const {
        if (from > to) std::swap(from, to);
        if (from >= s_.size()) return String();
        if (to > s_.size()) to = (unsigned int)s_.size();
        return String(s_.substr(from, to - from));
    }

    // modification
    void replace(char find, char repl) { for (auto &c : s_) if (c == find) c = repl; }
    void replace(const String &find, const String &repl);
    void remove(unsigned int index) { if (index < s_.size()) s_.erase(index); }
    void remove(unsigned int index, unsigned int count) { if (index < s_.size()) s_.erase(index, count); }
    void toLowerCase();
    void toUpperCase();
    void trim();
    void clear() { s_.clear(); }

    // conversion
    long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }
    float toFloat() const { return std::strtof(s_.c_str(), nullptr); }
    double toDouble() const { return std::strtod(s_.c_str(), nullptr); }
    void toCharArray(char *buf, unsigned int size, unsigned int index = 0) const;
    void getBytes(unsigned char *buf, unsigned int size, unsigned int index = 0) const {
        toCharArray(reinterpret_cast<char *>(buf), size, index);
    }

    // std::string-like helpers used by ArduinoJson
    const char *begin() const { return s_.c_str(); }
    const char *end() const { return s_.c_str() + s_.size(); }

private:
    std::string s_;
    static int npos(size_t p) { return p == std::string::npos ? -1 : (int)p; }
    static std::string fromUnsigned(unsigned long long v, unsigned char base);
    static std::string fromSigned(long long v, unsigned char base);
    static std::string fromDouble(double v, unsigned int decimals);
};

// Concatenation (Arduino uses StringSumHelper; plain value semantics suffice)
inline String operator+(const String &a, const String &b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, const char *b) { String r(a); r.concat(b); return r; }
inline String operator+(const char *a, const String &b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, char b) { String r(a); r.concat(b); return r; }
inline String operator+(char a, const String &b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, int b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, unsigned int b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, long b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, unsigned long b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, float b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, double b) { String r(a); r.concat(b); return r; }
inline bool operator==(const char *a, const String &b) { return b == a; }
inline bool operator!=(const char *a, const String &b) { return b != a; }

#define F(s) (s)
