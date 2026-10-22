#include "platform/platform.h"

#if GATEWAY_ETHERNET
#include <LittleFS.h>

bool Preferences::begin(const char* name, bool readOnly) {
    end();
    path_ = String("/prefs/") + name + ".json";
    readOnly_ = readOnly;
    if (!gw::fsLock()) return false;
    File file = LittleFS.open(path_, "r");
    // A missing or damaged file simply means defaults, as with an empty NVS namespace.
    if (!file || deserializeJson(doc_, file)) doc_.to<JsonObject>();
    if (file) file.close();
    gw::fsUnlock();
    open_ = true;
    return true;
}

void Preferences::end() {
    open_ = false;
    doc_.clear();
}

String Preferences::getString(const char* key, const String& fallback) {
    JsonVariantConst value = doc_[key];
    return open_ && value.is<const char*>() ? String(value.as<const char*>()) : fallback;
}

bool Preferences::save() {
    if (!gw::fsLock()) return false;
    LittleFS.mkdir("/prefs");
    // Write a temporary file first so a power cut cannot leave a half-written settings file.
    const String temp = path_ + ".tmp";
    File file = LittleFS.open(temp, "w");
    bool ok = file && serializeJson(doc_, file) > 0;
    if (file) file.close();
    ok = ok && LittleFS.rename(temp, path_);
    gw::fsUnlock();
    return ok;
}
#endif
