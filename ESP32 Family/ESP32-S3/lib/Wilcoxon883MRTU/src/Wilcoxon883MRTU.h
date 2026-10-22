#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include "Wilcoxon883MVersion.h"
#include "detail/RtuClient.h"
#include "detail/Discovery.h"

#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_RP2040)
#error "Wilcoxon883MRTU beta supports Arduino on ESP32 and RP2040/RP2350 only."
#endif

class Wilcoxon883MRTU {
public:
#if defined(ARDUINO_ARCH_RP2040)
    using Uart = SerialUART; // pins are assigned through SerialUART::setRX/setTX
#else
    using Uart = HardwareSerial;
#endif
    using Result = wilcoxon883m::Result;
    using Metrics = wilcoxon883m::Metrics;
    using Identity = wilcoxon883m::Identity;
    using SerialMode = wilcoxon883m::SerialMode;
    using DiscoveryOptions = wilcoxon883m::DiscoveryOptions;
    using DiscoveryState = wilcoxon883m::DiscoveryState;
    using DiscoveryResult = wilcoxon883m::DiscoveryResult;
    using DiscoveryProgress = wilcoxon883m::DiscoveryProgress;
    static const uint16_t MaxRegisters = 125;
    explicit Wilcoxon883MRTU(Uart& serial);
    // Configure the UART only; this does not change sensor settings.
    // directionPin -1 suits transceivers that switch direction themselves.
    bool begin(int rxPin, int txPin, int directionPin, uint32_t baud = 9600,
               uint8_t unit = 1, uint32_t serialFormat = SERIAL_8N1);
    Result readInputRegisters(uint16_t address, uint16_t count, uint16_t* values,
                              size_t capacity, uint32_t timeoutMs = 2000);
    Result readHoldingRegisters(uint16_t address, uint16_t count, uint16_t* values,
                                size_t capacity, uint32_t timeoutMs = 2000);
    Result writeSingleRegister(uint16_t address, uint16_t value, uint32_t timeoutMs = 2000);
    Result readMetrics(Metrics& metrics, uint32_t timeoutPerRequestMs = 2000);
    // Read-only discovery; call pollDiscovery() from loop() until it finishes.
    bool startDiscovery(int rxPin,int txPin,int directionPin,
                        const DiscoveryOptions& options = DiscoveryOptions());
    DiscoveryState pollDiscovery() { return discovery_.poll(); }
    void cancelDiscovery() { discovery_.cancel(); }
    const DiscoveryProgress& discoveryProgress() const { return discovery_.progress(); }
    const DiscoveryResult& discoveryResult() const { return discovery_.result(); }
    Result readIdentity(Identity& identity,uint32_t timeoutMs = 2000);
    static const char* serialModeText(SerialMode mode);
    static const char* discoveryStateText(DiscoveryState state);
    uint8_t exceptionCode() const { return client_.exceptionCode(); }
    static const char* resultText(Result result);
private:
    struct SerialTransport {
        Uart& serial;
        int direction = -1;
        explicit SerialTransport(Uart& value) : serial(value) {}
        uint32_t ms() { return millis(); }
        uint32_t us() { return micros(); }
        void idle() { yield(); delayMicroseconds(50); }
        int available() { return serial.available(); }
        int read() { return serial.read(); }
        size_t write(const uint8_t* data, size_t count) { return serial.write(data, count); }
#if defined(ARDUINO_ARCH_RP2040)
        void flush() { serial.flush(); }
#else
        void flush() { serial.flush(true); }
#endif
        void tx(bool enabled) { if (direction >= 0) digitalWrite(direction, enabled ? HIGH : LOW); }
    } transport_;
    wilcoxon883m::Client<SerialTransport> client_;
    struct DiscoveryProbe {
        Wilcoxon883MRTU& owner;
        explicit DiscoveryProbe(Wilcoxon883MRTU& value):owner(value){}
        uint32_t nowMs() { return millis(); }
        bool configure(const wilcoxon883m::Settings& settings);
        Result readHolding(uint16_t a,uint16_t n,uint16_t* out,size_t capacity,uint32_t timeout) {
            return owner.client_.readHolding(a,n,out,capacity,timeout);
        }
        Result readIdentity(Identity& out,uint32_t timeout) { return owner.client_.readIdentity(out,timeout); }
        bool probe(const wilcoxon883m::Settings& candidate,const DiscoveryOptions& options,DiscoveryResult& out) {
            return wilcoxon883m::probeSensor(*this,candidate,options,out);
        }
    } probe_;
    wilcoxon883m::Discovery<DiscoveryProbe> discovery_;
    int discoveryRx_=-1,discoveryTx_=-1,discoveryDirection_=-1;
    bool configureUart(int rx,int tx,int direction,uint32_t baud,uint8_t unit,uint32_t format);
    bool discovering() const { return discovery_.progress().state==DiscoveryState::Searching; }
    Wilcoxon883MRTU(const Wilcoxon883MRTU&) = delete;
    Wilcoxon883MRTU& operator=(const Wilcoxon883MRTU&) = delete;
};
