#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "Wilcoxon883MVersion.h"
#include "detail/TcpSession.h"
#include "detail/RegisterBlocks.h"

#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_RP2040)
#error "Wilcoxon883MTCP beta supports Arduino on ESP32 and RP2040/RP2350 only."
#endif

// Modbus TCP server (functions 03, 04, 06, 16) for publishing data to PLCs.
// It serves registers from a RegisterBank; it is not a transparent RTU bridge.
class Wilcoxon883MTCP {
public:
    using RegisterBank = wilcoxon883m::RegisterBank;
    template<size_t Capacity> using RegisterBlocks = wilcoxon883m::RegisterBlocks<Capacity>;
    using Stats = wilcoxon883m::TcpStats;
    using Timeouts = wilcoxon883m::TcpTimeouts;
    static const uint8_t MaxClients = 8;
    explicit Wilcoxon883MTCP(RegisterBank& bank) : bank_(bank) {}
    ~Wilcoxon883MTCP() { end(); }
    // Start listening once the network is up. unitId 0 accepts any unit ID.
    bool begin(uint16_t port = 502, uint8_t maxClients = 4, uint8_t unitId = 0);
    void end();
    // Call often from one task. Handles at most one request per client per call.
    void poll();
    void setTimeouts(const Timeouts& timeouts) { timeouts_ = timeouts; }
    bool running() const { return running_; }
    uint8_t clientCount() const;
    const Stats& stats() const { return stats_; }
private:
    struct Connection {
        WiFiClient client;
        bool connected() { return client.connected(); }
        int available() { return client.available(); }
        int read() { return client.read(); }
        int send(const uint8_t* data, size_t size);
        void stop() { client.stop(); }
    };
    RegisterBank& bank_;
    WiFiServer server_;
    bool running_ = false;
    wilcoxon883m::TcpSession<Connection> sessions_[MaxClients];
    uint8_t maxClients_ = 0, unitId_ = 0;
    Timeouts timeouts_;
    Stats stats_;
    Wilcoxon883MTCP(const Wilcoxon883MTCP&) = delete;
    Wilcoxon883MTCP& operator=(const Wilcoxon883MTCP&) = delete;
};
