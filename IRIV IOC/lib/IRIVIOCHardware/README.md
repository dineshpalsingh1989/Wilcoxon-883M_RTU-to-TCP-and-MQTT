# IRIVIOCHardware

Experimental 0.1.0-alpha.1 Arduino-Pico library containing configurable W5500 Ethernet setup and a bounded LittleFS settings store. Hardware validation is pending. It reuses Arduino-Pico's Ethernet/filesystem support and ArduinoJson 6.

`IRIVIOCHardware.h` provides `irivioc::Ethernet(cs, spi, interruptPin)`, `begin(hostname)`, `connected()` and `localIP()`. The caller configures the existing board SPI pins and reset pin before `begin()`. There is no automatic static-IP fallback. Call networking only from Arduino setup/loop on core 0. Keep the hostname alive. This initial library does not control relays or change board wiring.

`IRIVIOCSettings.h` provides `irivioc::Settings(lock, unlock)`, extracted from the gateway's Preferences-compatible implementation. It supports the existing typed getters/setters, `begin(namespace, readOnly)` and `end()`. Namespace names allow up to 32 letters, numbers, hyphens or underscores. Each namespace uses a fixed 1024-byte JSON document. Writes reject document overflow and report filesystem failures; replacement uses a temporary file and rename. This is not a guarantee against every power-loss/storage failure.

Mount LittleFS explicitly before use. The library never formats storage. Both lock callbacks are required and must coordinate with every other filesystem user. No-op callbacks are suitable only for a single-threaded application. A Settings object must have one owner; filesystem locking does not make concurrent access to its JSON document safe. Missing or malformed files return defaults, matching the existing gateway behavior. Setters return zero on failure; do not report successful persistence unless the return value indicates success.

See the sibling [secure MQTT example](../W5500SecureMqtt/examples/IRIVSecurePublish/main.cpp) for both APIs and its [compile project](../W5500SecureMqtt/tests/iriv_compile/platformio.ini). Existing gateway files remain unchanged; integration requires separate scheduling and persistence verification.
