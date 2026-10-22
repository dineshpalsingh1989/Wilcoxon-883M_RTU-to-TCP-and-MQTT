#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include "detail/RtuClient.h"
#include "WilcoxonILA210Registers.h"

#if !defined(ARDUINO_ARCH_ESP32)
#error "WilcoxonILA210RTU currently supports Arduino ESP32 only."
#endif

// One calling task. UART pins/configuration remain owned by the application.
class WilcoxonILA210RTU {
public:
    static constexpr uint16_t MaxRegisters = 64;
    static constexpr uint8_t ku8MBSuccess = 0;
    using Result = wilcoxonila210::Result;
    WilcoxonILA210RTU() : client_(transport_) {}
    bool begin(uint8_t unit, HardwareSerial& serial, uint32_t baud) {
        transport_.serial = &serial; valid_ = 0;
        client_.reset();
        const bool ok = client_.configure(baud, unit, 10); // Existing 8N1 format.
        last_ = ok ? Result::Ok : Result::InvalidArgument;
        return ok;
    }
    void preTransmission(void (*callback)()) { transport_.before = callback; }
    void postTransmission(void (*callback)()) { transport_.after = callback; }
    uint8_t readInputRegisters(uint16_t address, uint16_t count) { return read(false, address, count); }
    uint8_t readHoldingRegisters(uint16_t address, uint16_t count) { return read(true, address, count); }
    uint8_t readMetric(uint8_t index, float& value) {
        if (index >= wilcoxonila210::MetricCount) { valid_ = 0; return status(Result::InvalidArgument); }
        const auto& metric = wilcoxonila210::MetricRegisters[index];
        const uint8_t code = readInputRegisters(metric.inputAddress, 1);
        if (!code) value = static_cast<int16_t>(response_[0]) * metric.readScale;
        return code;
    }
    uint8_t writeSingleRegister(uint16_t address, uint16_t value) {
        valid_ = 0; return status(client_.writeSingle(address, value, 2000));
    }
    uint16_t getResponseBuffer(uint16_t index) const { return index < valid_ ? response_[index] : 0; }
    Result lastResult() const { return last_; }
    uint8_t exceptionCode() const { return last_ == Result::Exception ? client_.exceptionCode() : 0; }
private:
    struct Transport {
        HardwareSerial* serial = nullptr;
        void (*before)() = nullptr;
        void (*after)() = nullptr;
        uint32_t ms() { return millis(); }
        uint32_t us() { return micros(); }
        void idle() { yield(); delayMicroseconds(50); }
        int available() { return serial->available(); }
        int read() { return serial->read(); }
        size_t write(const uint8_t* data, size_t count) { return serial->write(data, count); }
        void flush() { serial->flush(true); }
        void tx(bool active) { if (active) { if (before) before(); } else if (after) after(); }
    } transport_;
    wilcoxonila210::Client<Transport> client_;
    uint16_t response_[MaxRegisters] = {};
    uint16_t valid_ = 0;
    Result last_ = Result::InvalidArgument;
    uint8_t read(bool holding, uint16_t address, uint16_t count) {
        valid_ = 0;
        if (!count || count > MaxRegisters) return status(Result::InvalidArgument);
        const Result result = holding ? client_.readHolding(address, count, response_, MaxRegisters, 2000)
                                      : client_.readInput(address, count, response_, MaxRegisters, 2000);
        if (result == Result::Ok) valid_ = count;
        return status(result);
    }
    uint8_t status(Result value) {
        last_ = value;
        switch (value) {
            case Result::Ok: return 0;
            case Result::Exception: return client_.exceptionCode();
            case Result::Timeout: return 0xe2;
            case Result::Crc: return 0xe3;
            case Result::InvalidArgument: return 0xe4;
            default: return 0xe0;
        }
    }
};
