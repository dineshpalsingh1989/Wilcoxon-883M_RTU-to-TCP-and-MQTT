#pragma once
#include <stdint.h>
class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : value{a, b, c, d} {}
    uint8_t value[4] = {0, 0, 0, 0};
};
