#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include "detail/RtuClient.h"

#if !defined(ARDUINO_ARCH_ESP32)
#error "Wilcoxon883MRTU beta supports Arduino on ESP32 only."
#endif

class Wilcoxon883MRTU {
public:
    using Result = wilcoxon883m::Result;
    using Metrics = wilcoxon883m::Metrics;
    static const uint16_t MaxRegisters = 125;
    explicit Wilcoxon883MRTU(HardwareSerial& serial);
    // Configure the ESP32 UART only; this does not change sensor settings.
    bool begin(int rxPin, int txPin, int directionPin, uint32_t baud = 9600,
               uint8_t unit = 1, uint32_t serialFormat = SERIAL_8N1);
    Result readInputRegisters(uint16_t address, uint16_t count, uint16_t* values,
                              size_t capacity, uint32_t timeoutMs = 2000);
    Result readHoldingRegisters(uint16_t address, uint16_t count, uint16_t* values,
                                size_t capacity, uint32_t timeoutMs = 2000);
    Result writeSingleRegister(uint16_t address, uint16_t value, uint32_t timeoutMs = 2000);
    Result readMetrics(Metrics& metrics, uint32_t timeoutPerRequestMs = 2000);
    uint8_t exceptionCode() const { return client_.exceptionCode(); }
    static const char* resultText(Result result);
private:
    struct SerialTransport {
        HardwareSerial& serial;
        int direction = -1;
        explicit SerialTransport(HardwareSerial& value) : serial(value) {}
        uint32_t ms() { return millis(); }
        uint32_t us() { return micros(); }
        void idle() { yield(); delayMicroseconds(50); }
        int available() { return serial.available(); }
        int read() { return serial.read(); }
        size_t write(const uint8_t* data, size_t count) { return serial.write(data, count); }
        void flush() { serial.flush(true); }
        void tx(bool enabled) { digitalWrite(direction, enabled ? HIGH : LOW); }
    } transport_;
    wilcoxon883m::Client<SerialTransport> client_;
    Wilcoxon883MRTU(const Wilcoxon883MRTU&) = delete;
    Wilcoxon883MRTU& operator=(const Wilcoxon883MRTU&) = delete;
};
