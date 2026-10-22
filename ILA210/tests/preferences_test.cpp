#include "platform/fs_preferences.h"
#include <cassert>
#include <iostream>
#include <map>
std::map<std::string, std::map<std::string, String>> Preferences::storage;
bool Preferences::failCommit = false;
unsigned Preferences::writes = 0;

int main() {
    GatewayPreferences prefs;
    Preferences::storage["mqtt-cfg"]["pass"] = "legacy-password";
    assert(prefs.begin("mqtt-cfg", false));
    assert(prefs.getString("pass") == "legacy-password");
    prefs.putString("server", "staged-only");
    assert(Preferences::writes == 0);
    prefs.end();
    assert(prefs.begin("mqtt-cfg", false));
    assert(prefs.getString("server", "missing") == "missing");
    const String password = prefs.getString("pass");
    assert(prefs.putString("server", "broker") == 6);
    prefs.putString("pass", password);
    prefs.putInt("port", 1883);
    assert(prefs.commit());
    assert(Preferences::writes == 1); // Complete namespace, one NVS write.
    prefs.end();
    assert(prefs.begin("mqtt-cfg", true));
    assert(prefs.getString("server") == "broker");
    assert(prefs.getString("pass") == password);
    assert(prefs.getInt("port") == 1883);
    assert(!prefs.putInt("port", 999));
    assert(!prefs.commit());
    prefs.end();

    // Replacing many distinct strings in one transaction must not accumulate
    // retired string allocations in ArduinoJson's monotonic memory pool.
    assert(prefs.begin("mqtt-cfg", false));
    for (unsigned n = 0; n < 50; ++n) {
        String host(std::string(253, char('A' + n % 26)));
        assert(prefs.putString("server", host) == 253);
        assert(prefs.putString("user", String(std::string(128, 'u'))) == 128);
        assert(prefs.putString("pass", String(std::string(128, 'p'))) == 128);
        assert(prefs.putString("topic", String(std::string(128, 't'))) == 128);
    }
    assert(prefs.commit());
    prefs.end();
    const String before = Preferences::storage["mqtt-cfg"]["__snapshot"];
    assert(prefs.begin("mqtt-cfg", false));
    prefs.putString("server", "changed");
    Preferences::failCommit = true;
    assert(!prefs.commit());
    assert(Preferences::storage["mqtt-cfg"]["__snapshot"] == before);
    Preferences::failCommit = false;
    prefs.end();

    assert(prefs.begin("mqtt-cfg", false));
    assert(prefs.putString("server", String(std::string(2200, 'x'))) == 0);
    assert(!prefs.commit());
    prefs.end();
    assert(Preferences::storage["mqtt-cfg"]["__snapshot"] == before);
    assert(prefs.begin("mqtt-cfg", false));
    // Plenty of pool room but serialized JSON exceeds the separate byte bound.
    assert(prefs.putString("server", String(std::string(1600, 'x'))) == 0);
    assert(!prefs.commit());
    prefs.end();
    assert(Preferences::storage["mqtt-cfg"]["__snapshot"] == before);

    Preferences::storage["broken"]["__snapshot"] = "{broken";
    assert(!prefs.begin("broken", false));
    assert(!prefs.commit());
    assert(prefs.getString("server", "default") == "default");
    assert(!prefs.begin("../unsafe", false));
    std::cout << "GatewayPreferences transaction, migration, repeated-edit, overflow and NVS failure tests PASS\n";
}
