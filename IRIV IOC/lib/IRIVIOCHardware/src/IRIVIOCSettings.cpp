#include "IRIVIOCSettings.h"

#include <LittleFS.h>
#include <ctype.h>

namespace irivioc {
bool Settings::begin(const char* name, bool readOnly) {
    end();
    if (!name || !*name || strlen(name) > 32) return false;
    for (const char* p = name; *p; ++p)
        if (!isalnum(static_cast<unsigned char>(*p)) && *p != '-' && *p != '_') return false;
    path_ = String("/prefs/") + name + ".json";
    readOnly_ = readOnly;
    if (!(lock_ && unlock_ && lock_())) return false;
    File file = LittleFS.open(path_, "r");
    // A missing or damaged file simply means defaults, as with an empty NVS namespace.
    if (!file || deserializeJson(doc_, file)) doc_.to<JsonObject>();
    if (file) file.close();
    unlock_();
    open_ = true;
    return true;
}

void Settings::end() {
    open_ = false;
    doc_.clear();
}

String Settings::getString(const char* key, const String& fallback) {
    JsonVariantConst value = doc_[key];
    return open_ && value.is<const char*>() ? String(value.as<const char*>()) : fallback;
}

bool Settings::save() {
    if (!(lock_ && unlock_ && lock_())) return false;
    LittleFS.mkdir("/prefs");
    // Write a temporary file first so a power cut cannot leave a half-written settings file.
    const String temp = path_ + ".tmp";
    File file = LittleFS.open(temp, "w");
    bool ok = file && serializeJson(doc_, file) == measureJson(doc_);
    if (file) {
        file.flush();
        ok = ok && !file.getWriteError();
        file.close();
    }
    ok = ok && LittleFS.rename(temp, path_);
    unlock_();
    return ok;
}
} // namespace irivioc
