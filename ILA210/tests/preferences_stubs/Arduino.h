#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
class String {
    std::string value_;
public:
    String(const char* value = "") : value_(value ? value : "") {}
    String(const std::string& value) : value_(value) {}
    const char* c_str() const { return value_.c_str(); }
    size_t length() const { return value_.length(); }
    bool reserve(size_t size) { value_.reserve(size); return true; }
    bool concat(const char* value) { value_ += value; return true; }
    friend String operator+(const String& a, const String& b) { return String(a.value_ + b.value_); }
    friend bool operator==(const String& a, const String& b) { return a.value_ == b.value_; }
    friend bool operator!=(const String& a, const String& b) { return !(a == b); }
};
