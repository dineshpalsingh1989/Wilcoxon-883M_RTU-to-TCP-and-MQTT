#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#if !defined(ARDUINO_ARCH_RP2040)
#include <Preferences.h>
#endif

// put* stages a complete namespace; only commit() publishes it. end() discards.
class GatewayPreferences {
public:
    bool begin(const char* name, bool readOnly = false);
    void end();
    bool commit();
#if defined(ARDUINO_ARCH_RP2040)
#define GW_GETTER(method, type, initial) type method(const char* key, type fallback = initial) { return get<type>(key, fallback); }
#else
#define GW_GETTER(method, type, initial) type method(const char* key, type fallback = initial) { \
    if (open_ && legacy_ && !doc_.containsKey(key)) return native_.method(key, fallback); \
    return get<type>(key, fallback); }
#endif
    GW_GETTER(getBool, bool, false)
    GW_GETTER(getUChar, uint8_t, 0)
    GW_GETTER(getUShort, uint16_t, 0)
    GW_GETTER(getInt, int32_t, 0)
    GW_GETTER(getUInt, uint32_t, 0)
    GW_GETTER(getLong, int32_t, 0)
    GW_GETTER(getFloat, float, NAN)
    GW_GETTER(getULong, uint32_t, 0)
#undef GW_GETTER
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
    static constexpr size_t Capacity = 2048, MaxSerialized = 1536;
    template <typename T> T get(const char* key, T fallback) {
        JsonVariantConst value = doc_[key];
        return open_ && value.is<T>() ? value.as<T>() : fallback;
    }
    template <typename T> bool put(const char* key, T value) {
        if (!open_ || readOnly_ || failed_ || !key || !*key) return false;
        // ArduinoJson 6 retains old strings on replacement. Compact a fresh
        // candidate before adding the new value so repeated edits stay bounded.
        StaticJsonDocument<Capacity> next;
        next.set(doc_);
        next.remove(key);
        next.garbageCollect();
        next[key] = value;
        if (next.overflowed() || measureJson(next) > MaxSerialized) { failed_ = true; return false; }
        doc_.clear();
        if (!doc_.set(next)) { failed_ = true; return false; }
        return true;
    }
    bool load(const String& json);
    String path_;
    bool open_ = false, readOnly_ = true, failed_ = false, legacy_ = false;
    StaticJsonDocument<Capacity> doc_;
#if !defined(ARDUINO_ARCH_RP2040)
    ::Preferences native_;
#endif
};
