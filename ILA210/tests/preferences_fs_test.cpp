#include "platform/fs_preferences.h"
#include <LittleFS.h>
#include <cassert>
#include <iostream>
std::map<std::string, String> File::storage;
bool File::shortWrite = false, File::corruptRead = false;
bool MockLittleFS::failRename = false;
MockLittleFS LittleFS;
namespace gw {
bool locked = false, failLock = false;
bool fsLock() { assert(!locked); if (failLock) return false; locked = true; return true; }
void fsUnlock() { assert(locked); locked = false; }
}
int main() {
    GatewayPreferences prefs;
    File::storage["/prefs/mqtt-cfg.json"] = "{\"server\":\"old\",\"port\":1883}";
    assert(prefs.begin("mqtt-cfg", false));
    assert(prefs.getString("server") == "old");
    assert(prefs.getInt("port") == 1883);
    prefs.putString("server", "discard"); prefs.end();
    assert(prefs.begin("mqtt-cfg", false));
    assert(prefs.getString("server") == "old");
    prefs.putString("server", "new");
    const String before = File::storage["/prefs/mqtt-cfg.json"];
    File::shortWrite = true;
    assert(!prefs.commit()); File::shortWrite = false;
    assert(File::storage["/prefs/mqtt-cfg.json"] == before);
    assert(!LittleFS.exists("/prefs/mqtt-cfg.json.tmp"));
    File::corruptRead = true;
    assert(!prefs.commit()); File::corruptRead = false;
    assert(File::storage["/prefs/mqtt-cfg.json"] == before);
    MockLittleFS::failRename = true;
    assert(!prefs.commit()); MockLittleFS::failRename = false;
    assert(File::storage["/prefs/mqtt-cfg.json"] == before);
    gw::failLock = true;
    assert(!prefs.commit()); gw::failLock = false;
    assert(File::storage["/prefs/mqtt-cfg.json"] == before);
    assert(prefs.commit()); prefs.end();
    assert(prefs.begin("mqtt-cfg", true));
    assert(prefs.getString("server") == "new");
    assert(prefs.getInt("port") == 1883);
    prefs.end();
    File::storage["/prefs/broken.json"] = "{broken";
    assert(!prefs.begin("broken", false));
    assert(!prefs.commit());
    assert(!gw::locked);
    std::cout << "GatewayPreferences LittleFS short-write, readback, rename, lock and transaction tests PASS\n";
}
