#pragma once
#include <Arduino.h>
#include <map>
// Test seam models one NVS item replacement and an unsuccessful commit. It does
// not emulate flash or prove the physical NVS power-loss guarantees.
class Preferences {
    std::string name_;
    bool readonly_ = true, open_ = false;
public:
    static std::map<std::string, std::map<std::string, String>> storage;
    static bool failCommit;
    static unsigned writes;
    bool begin(const char* name, bool readonly) { name_ = name; readonly_ = readonly; open_ = true; return true; }
    void end() { open_ = false; }
    bool isKey(const char* key) { return storage[name_].count(key) != 0; }
    String getString(const char* key, const String& fallback = String()) { return isKey(key) ? storage[name_][key] : fallback; }
    size_t putString(const char* key, const String& value) {
        if (!open_ || readonly_ || failCommit) return 0;
        storage[name_][key] = value; ++writes; return value.length();
    }
#define MOCK_GET(method, type) type method(const char*, type fallback) { return fallback; }
    MOCK_GET(getBool, bool)
    MOCK_GET(getUChar, uint8_t)
    MOCK_GET(getUShort, uint16_t)
    MOCK_GET(getInt, int32_t)
    MOCK_GET(getUInt, uint32_t)
    MOCK_GET(getLong, int32_t)
    MOCK_GET(getFloat, float)
    MOCK_GET(getULong, uint32_t)
#undef MOCK_GET
};
