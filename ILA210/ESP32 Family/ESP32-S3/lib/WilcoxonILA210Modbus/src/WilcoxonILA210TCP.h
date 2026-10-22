#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "detail/HoldingCache.h"
#include "detail/TcpSession.h"
#include "detail/RegisterBlocks.h"

#if !defined(ARDUINO_ARCH_ESP32)
#error "WilcoxonILA210TCP supports Arduino ESP32 only."
#endif

// Modbus TCP holding-cache server (03, 06, 16); 04 has no mapped inputs.
// Writes affect the cache only; this is not a transparent RTU bridge.
class WilcoxonILA210TCP {
public:
    using RegisterBank = wilcoxonila210::RegisterBank;
    template<size_t Capacity> using RegisterBlocks = wilcoxonila210::RegisterBlocks<Capacity>;
    using Stats = wilcoxonila210::TcpStats;
    using Timeouts = wilcoxonila210::TcpTimeouts;
    static const uint8_t MaxClients = 8;
    WilcoxonILA210TCP() = default;
    bool server(uint16_t port = 502) { return begin(port, 8); }
    void task() { poll(); }
    bool addHreg(uint16_t start, uint16_t value, uint16_t count) { return bank_.add(start, value, count); }
    bool Hreg(uint16_t address, uint16_t value) { return bank_.set(address, value); }
    ~WilcoxonILA210TCP() { end(); }
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
    wilcoxonila210::HoldingCache bank_;
    WiFiServer server_;
    bool running_ = false;
    wilcoxonila210::TcpSession<Connection> sessions_[MaxClients];
    uint8_t maxClients_ = 0, unitId_ = 0;
    Timeouts timeouts_;
    Stats stats_;
    WilcoxonILA210TCP(const WilcoxonILA210TCP&) = delete;
    WilcoxonILA210TCP& operator=(const WilcoxonILA210TCP&) = delete;
};
