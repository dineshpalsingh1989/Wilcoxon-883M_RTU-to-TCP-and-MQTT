#include "platform/fs_preferences.h"
#include "platform/platform.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <LittleFS.h>
#endif

bool GatewayPreferences::load(const String& json) {
    return json.length() <= MaxSerialized && !deserializeJson(doc_, json) &&
           !doc_.overflowed() && doc_.is<JsonObject>();
}

bool GatewayPreferences::begin(const char* name, bool readOnly) {
    end();
    if (!name || !*name || strlen(name) > 15) return false;
    for (const char* p = name; *p; ++p)
        if (!(isalnum(static_cast<unsigned char>(*p)) || *p == '-' || *p == '_')) return false;
    readOnly_ = readOnly;
    doc_.to<JsonObject>();
#if defined(ARDUINO_ARCH_RP2040)
    path_ = String("/prefs/") + name + ".json";
    if (!gw::fsLock()) return false;
    const bool exists = LittleFS.exists(path_);
    File file = LittleFS.open(path_, "r");
    bool ok = !exists || (file && file.size() <= MaxSerialized && load(file.readString()));
    if (file) file.close();
    gw::fsUnlock();
    if (!ok) { doc_.clear(); return false; }
#else
    if (!native_.begin(name, readOnly)) return false;
    if (native_.isKey("__snapshot")) {
        if (!load(native_.getString("__snapshot", ""))) { native_.end(); doc_.clear(); return false; }
    } else legacy_ = true;
#endif
    open_ = true;
    return true;
}

void GatewayPreferences::end() {
#if !defined(ARDUINO_ARCH_RP2040)
    native_.end();
#endif
    open_ = false; readOnly_ = true; failed_ = false; legacy_ = false;
    doc_.clear(); path_ = "";
}

String GatewayPreferences::getString(const char* key, const String& fallback) {
#if !defined(ARDUINO_ARCH_RP2040)
    if (open_ && legacy_ && !doc_.containsKey(key)) return native_.getString(key, fallback);
#endif
    JsonVariantConst value = doc_[key];
    return open_ && value.is<const char*>() ? String(value.as<const char*>()) : fallback;
}

bool GatewayPreferences::commit() {
    if (!open_ || readOnly_ || failed_ || doc_.overflowed()) return false;
    const size_t required = measureJson(doc_);
    if (required > MaxSerialized) return false;
    String json;
    if (!json.reserve(required + 1) || serializeJson(doc_, json) != required) return false;
#if defined(ARDUINO_ARCH_RP2040)
    if (!gw::fsLock()) return false;
    LittleFS.mkdir("/prefs");
    const String temp = path_ + ".tmp";
    File file = LittleFS.open(temp, "w");
    bool ok = file && file.print(json) == required;
    if (file) file.close();
    // close() cannot report flash errors; verify before replacing the old file.
    if (ok) {
        file = LittleFS.open(temp, "r");
        ok = file && file.size() == required && file.readString() == json;
        if (file) file.close();
    }
    if (ok) ok = LittleFS.rename(temp, path_);
    if (!ok) LittleFS.remove(temp);
    gw::fsUnlock();
    return ok;
#else
    // Native putString checks nvs_set_str AND nvs_commit. One NVS item avoids
    // exposing a partially updated namespace after a power interruption.
    const bool ok = native_.putString("__snapshot", json) == required;
    if (ok) legacy_ = false;
    return ok;
#endif
}
