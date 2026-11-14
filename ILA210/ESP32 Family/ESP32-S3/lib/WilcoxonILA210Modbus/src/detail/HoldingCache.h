#pragma once
#include "TcpProtocol.h"

namespace wilcoxonila210 {
// Existing ILA gateway contract: holding registers 0..49, writable by PLCs.
// Single owner: configure before tasks start, then use from the network task.
class HoldingCache : public RegisterBank {
public:
    static constexpr uint16_t Capacity = 50;
    bool add(uint16_t address, uint16_t value, uint16_t count) {
        if (!count || uint32_t(address) + count > Capacity) return false;
        for (uint16_t i = 0; i < count; ++i) if (mapped_[address+i]) return false;
        for (uint16_t i = 0; i < count; ++i) { mapped_[address+i] = true; data_[address+i] = value; }
        return true;
    }
    bool set(uint16_t address, uint16_t value) {
        if (!valid(address, 1)) return false;
        data_[address] = value; return true;
    }
    uint8_t readRegisters(bool input, uint16_t address, uint16_t count, uint16_t* out) override {
        if (input || !out || !valid(address, count)) return IllegalAddress;
        memcpy(out, data_ + address, size_t(count) * sizeof(uint16_t)); return 0;
    }
    uint8_t writeRegisters(uint16_t address, uint16_t count, const uint16_t* values) override {
        if (!values || !valid(address, count)) return IllegalAddress;
        memcpy(data_ + address, values, size_t(count) * sizeof(uint16_t)); return 0;
    }
private:
    uint16_t data_[Capacity] = {};
    bool mapped_[Capacity] = {};
    bool valid(uint16_t address, uint16_t count) const {
        if (!count || uint32_t(address) + count > Capacity) return false;
        for (uint16_t i = 0; i < count; ++i) if (!mapped_[address+i]) return false;
        return true;
    }
};
}
