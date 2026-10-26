#pragma once
// Subset of Arduino's Client interface; bodies are defaults so no C++ runtime is needed.
#include "Arduino.h"
#include "IPAddress.h"
class Client {
public:
    virtual int connect(IPAddress, uint16_t) { return 0; }
    virtual int connect(const char*, uint16_t) { return 0; }
    virtual size_t write(const uint8_t*, size_t) { return 0; }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual void stop() {}
    virtual uint8_t connected() { return 0; }
};
