#pragma once
#include <stdint.h>
#include <stddef.h>
class HardwareSerial { public: int available(); int read(); size_t write(const uint8_t*, size_t); void flush(bool); };
