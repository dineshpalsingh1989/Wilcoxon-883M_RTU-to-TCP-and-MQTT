#pragma once
// Preferences-compatible settings store for boards without ESP32 NVS (RP2350).
// Each namespace is a small JSON file in LittleFS; every put writes the file at once,
// matching NVS behaviour where a saved value survives an immediate restart.
#include <Arduino.h>
#include <ArduinoJson.h>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false);
    void end();
    bool getBool(const char* key, bool fallback = false) { return get<bool>(key, fallback); }
    uint8_t getUChar(const char* key, uint8_t fallback = 0) { return get<uint8_t>(key, fallback); }
    uint16_t getUShort(const char* key, uint16_t fallback = 0) { return get<uint16_t>(key, fallback); }
    int32_t getInt(const char* key, int32_t fallback = 0) { return get<int32_t>(key, fallback); }
    uint32_t getUInt(const char* key, uint32_t fallback = 0) { return get<uint32_t>(key, fallback); }
    int32_t getLong(const char* key, int32_t fallback = 0) { return get<int32_t>(key, fallback); }
    float getFloat(const char* key, float fallback = NAN) { return get<float>(key, fallback); }
    uint32_t getULong(const char* key, uint32_t fallback = 0) { return get<uint32_t>(key, fallback); }
    String getString(const char* key, const String& fallback = String());
    size_t putBool(const char* key, bool value) { return put(key, value) ? 1 : 0; }
    size_t putUChar(const char* key, uint8_t value) { return put(key, value) ? 1 : 0; }
    size_t putUShort(const char* key, uint16_t value) { return put(key, value) ? 2 : 0; }
    size_t putInt(const char* key, int32_t value) { return put(key, value) ? 4 : 0; }
    size_t putUInt(const char* key, uint32_t value) { return put(key, value) ? 4 : 0; }
    size_t putLong(const char* key, int32_t value) { return put(key, value) ? 4 : 0; }
    size_t putFloat(const char* key, float value) { return put(key, value) ? 4 : 0; }
    size_t putULong(const char* key, uint32_t value) { return put(key, value) ? 4 : 0; }
    size_t putString(const char* key, const String& value) { return put(key, value) ? value.length() : 0; }

private:
    template <typename T> T get(const char* key, T fallback) {
        JsonVariantConst value = doc_[key];
        return open_ && value.is<T>() ? value.as<T>() : fallback;
    }
    template <typename T> bool put(const char* key, T value) {
        if (!open_ || readOnly_) return false;
        doc_[key] = value;
        return save();
    }
    bool save();
    String path_;
    bool open_ = false, readOnly_ = true;
    StaticJsonDocument<1024> doc_; // largest namespace (MQTT) holds 9 short values
};
