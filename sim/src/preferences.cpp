// Desktop Preferences (NVS) backed by <data dir>/preferences.json.
// Type rules follow ESP32 NVS: bool is stored as u8, int/long as i32,
// uint/ulong as u32, float/double/bytes as blobs.

#include <Preferences.h>
#include <ArduinoJson.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace {

enum class NvsType { I8, U8, I16, U16, I32, U32, I64, U64, STR, BLOB };

const char *typeName(NvsType t) {
    switch (t) {
        case NvsType::I8: return "i8";
        case NvsType::U8: return "u8";
        case NvsType::I16: return "i16";
        case NvsType::U16: return "u16";
        case NvsType::I32: return "i32";
        case NvsType::U32: return "u32";
        case NvsType::I64: return "i64";
        case NvsType::U64: return "u64";
        case NvsType::STR: return "str";
        case NvsType::BLOB: return "blob";
    }
    return "?";
}

bool typeFromName(const std::string &n, NvsType &out) {
    static const std::map<std::string, NvsType> m = {
        {"i8", NvsType::I8}, {"u8", NvsType::U8}, {"i16", NvsType::I16}, {"u16", NvsType::U16},
        {"i32", NvsType::I32}, {"u32", NvsType::U32}, {"i64", NvsType::I64}, {"u64", NvsType::U64},
        {"str", NvsType::STR}, {"blob", NvsType::BLOB}};
    auto it = m.find(n);
    if (it == m.end()) return false;
    out = it->second;
    return true;
}

struct Entry {
    NvsType type;
    int64_t i = 0;
    uint64_t u = 0;
    std::string bytes;  // STR text or BLOB data
};

using Namespace = std::map<std::string, Entry>;
std::map<std::string, Namespace> g_store;
bool g_loaded = false;

const size_t NVS_KEY_MAX = 15;

std::filesystem::path storePath() {
    return std::filesystem::path(sim::dataDir()) / "preferences.json";
}

std::string toHex(const std::string &data) {
    static const char *digits = "0123456789abcdef";
    std::string out;
    for (unsigned char c : data) {
        out += digits[c >> 4];
        out += digits[c & 0xF];
    }
    return out;
}

std::string fromHex(const std::string &hex) {
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out += char(std::stoi(hex.substr(i, 2), nullptr, 16));
    }
    return out;
}

void load() {
    if (g_loaded) return;
    g_loaded = true;
    std::ifstream in(storePath(), std::ios::binary);
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        printf("[SIM] WARNING: could not parse %s - starting with empty preferences\n",
               storePath().string().c_str());
        return;
    }
    for (JsonPair nsPair : doc.as<JsonObject>()) {
        Namespace &ns = g_store[nsPair.key().c_str()];
        for (JsonPair kv : nsPair.value().as<JsonObject>()) {
            Entry e;
            if (!typeFromName(kv.value()["type"] | "", e.type)) continue;
            JsonVariant v = kv.value()["value"];
            switch (e.type) {
                case NvsType::STR: e.bytes = v.as<std::string>(); break;
                case NvsType::BLOB: e.bytes = fromHex(v.as<std::string>()); break;
                case NvsType::U8: case NvsType::U16: case NvsType::U32: case NvsType::U64:
                    e.u = v.as<uint64_t>(); break;
                default: e.i = v.as<int64_t>(); break;
            }
            ns[kv.key().c_str()] = e;
        }
    }
}

void save() {
    JsonDocument doc;
    for (auto &[nsName, ns] : g_store) {
        JsonObject nsObj = doc[nsName].to<JsonObject>();
        for (auto &[key, e] : ns) {
            JsonObject o = nsObj[key].to<JsonObject>();
            o["type"] = typeName(e.type);
            switch (e.type) {
                case NvsType::STR: o["value"] = e.bytes; break;
                case NvsType::BLOB: o["value"] = toHex(e.bytes); break;
                case NvsType::U8: case NvsType::U16: case NvsType::U32: case NvsType::U64:
                    o["value"] = e.u; break;
                default: o["value"] = e.i; break;
            }
        }
    }
    std::filesystem::create_directories(sim::dataDir());
    std::string out;
    serializeJsonPretty(doc, out);
    std::ofstream f(storePath(), std::ios::binary | std::ios::trunc);
    f << out;
}

} // namespace

// ---------------------------------------------------------------------------

bool Preferences::begin(const char *name, bool readOnly, const char *) {
    if (started_) return false;
    if (!name || strlen(name) > NVS_KEY_MAX) {
        printf("[SIM] Preferences: namespace '%s' is longer than 15 chars - fails on ESP32\n", name ? name : "");
        return false;
    }
    load();
    // Like NVS, opening a namespace that doesn't exist yet read-only fails
    if (readOnly && g_store.find(name) == g_store.end()) return false;
    ns_ = name;
    readOnly_ = readOnly;
    started_ = true;
    if (!readOnly) g_store[name];  // real NVS creates the namespace on RW open
    return true;
}

void Preferences::end() { started_ = false; }

bool Preferences::clear() {
    if (!started_ || readOnly_) return false;
    g_store[ns_.c_str()].clear();
    save();
    return true;
}

bool Preferences::remove(const char *key) {
    if (!started_ || readOnly_ || !key) return false;
    bool erased = g_store[ns_.c_str()].erase(key) > 0;
    if (erased) save();
    return erased;
}

bool Preferences::isKey(const char *key) {
    if (!started_ || !key) return false;
    auto &ns = g_store[ns_.c_str()];
    return ns.find(key) != ns.end();
}

// --- internal helpers ------------------------------------------------------
namespace {

Entry *findEntry(const String &ns, const char *key) {
    auto nsIt = g_store.find(ns.c_str());
    if (nsIt == g_store.end() || !key) return nullptr;
    auto it = nsIt->second.find(key);
    return it == nsIt->second.end() ? nullptr : &it->second;
}

bool checkKey(const char *key) {
    if (!key || !*key) return false;
    if (strlen(key) > NVS_KEY_MAX) {
        printf("[SIM] Preferences: key '%s' is longer than 15 chars - this FAILS on ESP32\n", key);
        return false;
    }
    return true;
}

// Returns the entry if present and of the expected type; warns on mismatch
Entry *typed(const String &ns, const char *key, NvsType want) {
    Entry *e = findEntry(ns, key);
    if (e && e->type != want) {
        printf("[SIM] Preferences: '%s/%s' is stored as %s but read as %s - returns default on ESP32\n",
               ns.c_str(), key, typeName(e->type), typeName(want));
        return nullptr;
    }
    return e;
}

} // namespace

#define PUT_GUARD() if (!started_ || readOnly_ || !checkKey(key)) return 0

#define PUT_SIGNED(T, NT, SZ) { PUT_GUARD(); Entry e; e.type = NT; e.i = (int64_t)value; \
    g_store[ns_.c_str()][key] = e; save(); return SZ; }
#define PUT_UNSIGNED(T, NT, SZ) { PUT_GUARD(); Entry e; e.type = NT; e.u = (uint64_t)value; \
    g_store[ns_.c_str()][key] = e; save(); return SZ; }

size_t Preferences::putChar(const char *key, int8_t value) PUT_SIGNED(int8_t, NvsType::I8, 1)
size_t Preferences::putUChar(const char *key, uint8_t value) PUT_UNSIGNED(uint8_t, NvsType::U8, 1)
size_t Preferences::putShort(const char *key, int16_t value) PUT_SIGNED(int16_t, NvsType::I16, 2)
size_t Preferences::putUShort(const char *key, uint16_t value) PUT_UNSIGNED(uint16_t, NvsType::U16, 2)
size_t Preferences::putInt(const char *key, int32_t value) PUT_SIGNED(int32_t, NvsType::I32, 4)
size_t Preferences::putUInt(const char *key, uint32_t value) PUT_UNSIGNED(uint32_t, NvsType::U32, 4)
size_t Preferences::putLong(const char *key, int32_t value) PUT_SIGNED(int32_t, NvsType::I32, 4)
size_t Preferences::putULong(const char *key, uint32_t value) PUT_UNSIGNED(uint32_t, NvsType::U32, 4)
size_t Preferences::putLong64(const char *key, int64_t value) PUT_SIGNED(int64_t, NvsType::I64, 8)
size_t Preferences::putULong64(const char *key, uint64_t value) PUT_UNSIGNED(uint64_t, NvsType::U64, 8)
size_t Preferences::putBool(const char *key, bool value) PUT_UNSIGNED(uint8_t, NvsType::U8, 1)

size_t Preferences::putFloat(const char *key, float value) { return putBytes(key, &value, sizeof(value)); }
size_t Preferences::putDouble(const char *key, double value) { return putBytes(key, &value, sizeof(value)); }

size_t Preferences::putString(const char *key, const char *value) {
    PUT_GUARD();
    if (!value) return 0;
    Entry e;
    e.type = NvsType::STR;
    e.bytes = value;
    g_store[ns_.c_str()][key] = e;
    save();
    return strlen(value);
}

size_t Preferences::putBytes(const char *key, const void *value, size_t len) {
    PUT_GUARD();
    if (!value || !len) return 0;
    Entry e;
    e.type = NvsType::BLOB;
    e.bytes.assign(static_cast<const char *>(value), len);
    g_store[ns_.c_str()][key] = e;
    save();
    return len;
}

#define GET_SIGNED(T, NT) { if (!started_) return defaultValue; Entry *e = typed(ns_, key, NT); \
    return e ? (T)e->i : defaultValue; }
#define GET_UNSIGNED(T, NT) { if (!started_) return defaultValue; Entry *e = typed(ns_, key, NT); \
    return e ? (T)e->u : defaultValue; }

int8_t Preferences::getChar(const char *key, int8_t defaultValue) GET_SIGNED(int8_t, NvsType::I8)
uint8_t Preferences::getUChar(const char *key, uint8_t defaultValue) GET_UNSIGNED(uint8_t, NvsType::U8)
int16_t Preferences::getShort(const char *key, int16_t defaultValue) GET_SIGNED(int16_t, NvsType::I16)
uint16_t Preferences::getUShort(const char *key, uint16_t defaultValue) GET_UNSIGNED(uint16_t, NvsType::U16)
int32_t Preferences::getInt(const char *key, int32_t defaultValue) GET_SIGNED(int32_t, NvsType::I32)
uint32_t Preferences::getUInt(const char *key, uint32_t defaultValue) GET_UNSIGNED(uint32_t, NvsType::U32)
int32_t Preferences::getLong(const char *key, int32_t defaultValue) GET_SIGNED(int32_t, NvsType::I32)
uint32_t Preferences::getULong(const char *key, uint32_t defaultValue) GET_UNSIGNED(uint32_t, NvsType::U32)
int64_t Preferences::getLong64(const char *key, int64_t defaultValue) GET_SIGNED(int64_t, NvsType::I64)
uint64_t Preferences::getULong64(const char *key, uint64_t defaultValue) GET_UNSIGNED(uint64_t, NvsType::U64)

bool Preferences::getBool(const char *key, bool defaultValue) {
    return getUChar(key, defaultValue ? 1 : 0) != 0;
}

float Preferences::getFloat(const char *key, float defaultValue) {
    float v = defaultValue;
    return getBytes(key, &v, sizeof(v)) == sizeof(v) ? v : defaultValue;
}

double Preferences::getDouble(const char *key, double defaultValue) {
    double v = defaultValue;
    return getBytes(key, &v, sizeof(v)) == sizeof(v) ? v : defaultValue;
}

size_t Preferences::getString(const char *key, char *value, size_t maxLen) {
    if (!started_) return 0;
    Entry *e = typed(ns_, key, NvsType::STR);
    if (!e) return 0;
    size_t need = e->bytes.size() + 1;
    if (!value) return need;
    if (need > maxLen) return 0;
    memcpy(value, e->bytes.c_str(), need);
    return need;
}

String Preferences::getString(const char *key, const String &defaultValue) {
    if (!started_) return defaultValue;
    Entry *e = typed(ns_, key, NvsType::STR);
    return e ? String(e->bytes) : defaultValue;
}

size_t Preferences::getBytesLength(const char *key) {
    if (!started_) return 0;
    Entry *e = typed(ns_, key, NvsType::BLOB);
    return e ? e->bytes.size() : 0;
}

size_t Preferences::getBytes(const char *key, void *buf, size_t maxLen) {
    if (!started_) return 0;
    Entry *e = typed(ns_, key, NvsType::BLOB);
    if (!e) return 0;
    if (!buf) return e->bytes.size();
    if (e->bytes.size() > maxLen) return 0;
    memcpy(buf, e->bytes.data(), e->bytes.size());
    return e->bytes.size();
}
