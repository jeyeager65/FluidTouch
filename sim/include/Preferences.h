// Desktop implementation of the ESP32 Preferences (NVS) library.
//
// Values are persisted to <data dir>/preferences.json. Like real NVS, each
// key remembers its type, and reading it back as a different type returns
// the default (with a warning) - so type mismatches show up in the simulator
// the same way they would on hardware. Namespace/key names longer than 15
// characters are rejected, matching NVS.
#pragma once

#include <Arduino.h>

class Preferences {
public:
    Preferences() = default;
    ~Preferences() { end(); }

    bool begin(const char *name, bool readOnly = false, const char *partition_label = nullptr);
    void end();

    bool clear();
    bool remove(const char *key);
    bool isKey(const char *key);
    size_t freeEntries() { return 500; }

    size_t putChar(const char *key, int8_t value);
    size_t putUChar(const char *key, uint8_t value);
    size_t putShort(const char *key, int16_t value);
    size_t putUShort(const char *key, uint16_t value);
    size_t putInt(const char *key, int32_t value);
    size_t putUInt(const char *key, uint32_t value);
    size_t putLong(const char *key, int32_t value);
    size_t putULong(const char *key, uint32_t value);
    size_t putLong64(const char *key, int64_t value);
    size_t putULong64(const char *key, uint64_t value);
    size_t putFloat(const char *key, float value);
    size_t putDouble(const char *key, double value);
    size_t putBool(const char *key, bool value);
    size_t putString(const char *key, const char *value);
    size_t putString(const char *key, const String &value) { return putString(key, value.c_str()); }
    size_t putBytes(const char *key, const void *value, size_t len);

    int8_t getChar(const char *key, int8_t defaultValue = 0);
    uint8_t getUChar(const char *key, uint8_t defaultValue = 0);
    int16_t getShort(const char *key, int16_t defaultValue = 0);
    uint16_t getUShort(const char *key, uint16_t defaultValue = 0);
    int32_t getInt(const char *key, int32_t defaultValue = 0);
    uint32_t getUInt(const char *key, uint32_t defaultValue = 0);
    int32_t getLong(const char *key, int32_t defaultValue = 0);
    uint32_t getULong(const char *key, uint32_t defaultValue = 0);
    int64_t getLong64(const char *key, int64_t defaultValue = 0);
    uint64_t getULong64(const char *key, uint64_t defaultValue = 0);
    float getFloat(const char *key, float defaultValue = NAN);
    double getDouble(const char *key, double defaultValue = NAN);
    bool getBool(const char *key, bool defaultValue = false);
    size_t getString(const char *key, char *value, size_t maxLen);
    String getString(const char *key, const String &defaultValue = String());
    size_t getBytesLength(const char *key);
    size_t getBytes(const char *key, void *buf, size_t maxLen);

private:
    bool started_ = false;
    bool readOnly_ = false;
    String ns_;
};
