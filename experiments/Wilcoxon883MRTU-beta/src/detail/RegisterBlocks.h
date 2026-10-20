#pragma once
#include "TcpProtocol.h"

namespace wilcoxon883m {

// Serves existing uint16_t/int16_t arrays at chosen Modbus addresses without
// copying them. Optional lock/unlock callbacks (for example a FreeRTOS mutex)
// wrap each request so a reply never mixes two updates. A request must fit
// inside one block. The write callback runs after unlock.
template<size_t Capacity> class RegisterBlocks : public RegisterBank {
    struct Block { uint16_t start, count; uint16_t* data; bool input, writable; };
    Block blocks_[Capacity] = {};
    size_t used_ = 0;
    bool (*lock_)(void*) = nullptr;
    void (*unlock_)(void*) = nullptr;
    void* lockContext_ = nullptr;
    void (*written_)(uint16_t, uint16_t, void*) = nullptr;
    void* writtenContext_ = nullptr;
    Block* find(bool input, uint16_t address, uint16_t count) {
        for (size_t i = 0; i < used_; ++i) {
            Block& b = blocks_[i];
            if (b.input == input && address >= b.start &&
                uint32_t(address) + count <= uint32_t(b.start) + b.count) return &b;
        }
        return nullptr;
    }
    bool add(bool input, uint16_t start, uint16_t* data, uint16_t count, bool writable) {
        if (used_ == Capacity || !data || !range(start, count)) return false;
        for (size_t i = 0; i < used_; ++i) {
            const Block& b = blocks_[i];
            if (b.input == input && start < uint32_t(b.start) + b.count &&
                b.start < uint32_t(start) + count) return false;
        }
        blocks_[used_++] = Block{start, count, data, input, writable};
        return true;
    }
public:
    // Function 03/06/16 registers. Writable blocks accept PLC writes.
    bool addHolding(uint16_t start, uint16_t* data, uint16_t count, bool writable = false) {
        return add(false, start, data, count, writable);
    }
    bool addHolding(uint16_t start, int16_t* data, uint16_t count, bool writable = false) {
        return add(false, start, reinterpret_cast<uint16_t*>(data), count, writable);
    }
    // Function 04 registers; always read-only.
    bool addInput(uint16_t start, uint16_t* data, uint16_t count) {
        return add(true, start, data, count, false);
    }
    bool addInput(uint16_t start, int16_t* data, uint16_t count) {
        return add(true, start, reinterpret_cast<uint16_t*>(data), count, false);
    }
    // lock returns false when it cannot obtain the lock; the client gets DeviceBusy.
    void setLock(bool (*lock)(void*), void (*unlock)(void*), void* context = nullptr) {
        lock_ = lock; unlock_ = unlock; lockContext_ = context;
    }
    void onWrite(void (*written)(uint16_t address, uint16_t count, void* context), void* context = nullptr) {
        written_ = written; writtenContext_ = context;
    }
    uint8_t readRegisters(bool input, uint16_t address, uint16_t count, uint16_t* out) override {
        Block* b = find(input, address, count);
        if (!b) return IllegalAddress;
        if (lock_ && !lock_(lockContext_)) return DeviceBusy;
        memcpy(out, b->data + (address - b->start), size_t(count) * 2);
        if (unlock_) unlock_(lockContext_);
        return 0;
    }
    uint8_t writeRegisters(uint16_t address, uint16_t count, const uint16_t* values) override {
        Block* b = find(false, address, count);
        if (!b || !b->writable) return IllegalAddress;
        if (lock_ && !lock_(lockContext_)) return DeviceBusy;
        memcpy(b->data + (address - b->start), values, size_t(count) * 2);
        if (unlock_) unlock_(lockContext_);
        if (written_) written_(address, count, writtenContext_);
        return 0;
    }
};

} // namespace wilcoxon883m
