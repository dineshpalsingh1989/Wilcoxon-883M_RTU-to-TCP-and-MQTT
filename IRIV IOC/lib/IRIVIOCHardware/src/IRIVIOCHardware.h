#pragma once
#include <Arduino.h>
#include <W5500lwIP.h>

namespace irivioc {
// Caller supplies board wiring. Does not change pin assignments, reset the chip,
// mount/format storage, or silently switch to a static IP.
class Ethernet {
public:
    Ethernet(int cs, SPIClass& spi, int interruptPin = -1)
        : driver_(cs, spi, interruptPin) {}
    Ethernet(const Ethernet&) = delete;
    Ethernet& operator=(const Ethernet&) = delete;
    bool begin(const char* hostname) {
        driver_.setHostname(hostname);
        return driver_.begin();
    }
    bool connected() { return driver_.connected(); }
    IPAddress localIP() { return driver_.localIP(); }
private:
    Wiznet5500lwIP driver_;
};
}
