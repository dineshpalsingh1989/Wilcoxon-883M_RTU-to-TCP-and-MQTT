#pragma once
#include "RtuProtocol.h"

// Portable Modbus TCP server subset; no Arduino or third-party Modbus dependencies.
namespace wilcoxon883m {

constexpr size_t MaxTcpAdu = 260; // 7-byte MBAP header + 253-byte PDU.
constexpr uint16_t MaxWrite = 123;
constexpr uint8_t IllegalFunction = 1, IllegalAddress = 2, IllegalValue = 3,
                  DeviceFailure = 4, DeviceBusy = 6;

// Registers served to TCP clients. Each call covers a whole request range so an
// implementation can lock once and reply with one consistent snapshot. Return 0
// on success or a Modbus exception code; unknown codes are sent as DeviceFailure.
// A write must either store every value or none.
class RegisterBank {
public:
    virtual uint8_t readRegisters(bool input, uint16_t address, uint16_t count, uint16_t* out) {
        (void)input; (void)address; (void)count; (void)out; return IllegalAddress;
    }
    virtual uint8_t writeRegisters(uint16_t address, uint16_t count, const uint16_t* values) {
        (void)address; (void)count; (void)values; return IllegalAddress;
    }
protected:
    ~RegisterBank() {} // Non-owning interface; never delete through a bank pointer.
};

inline size_t exceptionPdu(uint8_t function, uint8_t code, uint8_t* out) {
    if (code != IllegalFunction && code != IllegalAddress && code != IllegalValue && code != DeviceBusy)
        code = DeviceFailure;
    out[0] = function | 0x80; out[1] = code; return 2;
}
// Functions 03/04 read, 06/16 write. Returns the reply PDU size.
inline size_t servePdu(const uint8_t* request, size_t size, uint8_t* out, RegisterBank& bank) {
    if (!request || !out || size < 1 || size > 253) return 0;
    const uint8_t fn = request[0];
    if (fn != 3 && fn != 4 && fn != 6 && fn != 16) return exceptionPdu(fn, IllegalFunction, out);
    if (size < 5 || (fn != 16 && size != 5)) return exceptionPdu(fn, IllegalValue, out);
    const uint16_t address = word(request + 1), value = word(request + 3);
    uint16_t values[MaxRead];
    if (fn == 3 || fn == 4) {
        if (value < 1 || value > MaxRead) return exceptionPdu(fn, IllegalValue, out);
        if (!range(address, value)) return exceptionPdu(fn, IllegalAddress, out);
        const uint8_t code = bank.readRegisters(fn == 4, address, value, values);
        if (code) return exceptionPdu(fn, code, out);
        out[0] = fn; out[1] = uint8_t(value * 2);
        for (uint16_t i = 0; i < value; ++i) put(out + 2 + 2 * i, values[i]);
        return 2 + 2 * size_t(value);
    }
    const uint16_t count = fn == 6 ? 1 : value;
    if (fn == 16 && (size < 6 || count < 1 || count > MaxWrite ||
                     request[5] != count * 2 || size != size_t(6 + count * 2)))
        return exceptionPdu(fn, IllegalValue, out);
    if (!range(address, count)) return exceptionPdu(fn, IllegalAddress, out);
    if (fn == 6) values[0] = value;
    else for (uint16_t i = 0; i < count; ++i) values[i] = word(request + 6 + 2 * i);
    const uint8_t code = bank.writeRegisters(address, count, values);
    if (code) return exceptionPdu(fn, code, out);
    memcpy(out, request, 5); return 5;
}
// unitFilter 0 accepts any unit identifier, as a TCP-only server normally does.
// Returns the reply ADU size, or 0 when the request is ignored without reply.
inline size_t serveTcp(const uint8_t* request, size_t size, uint8_t unitFilter,
                       uint8_t* out, size_t capacity, RegisterBank& bank) {
    if (!request || !out || size < 8 || size > MaxTcpAdu || capacity < MaxTcpAdu ||
        word(request + 2) != 0 || word(request + 4) != size - 6) return 0;
    if (unitFilter && request[6] != unitFilter) return 0;
    memcpy(out, request, 7);
    const size_t pduSize = servePdu(request + 7, size - 7, out + 7, bank);
    if (!pduSize) return 0;
    put(out + 4, uint16_t(pduSize + 1)); return pduSize + 7;
}
// Collects one ADU from a byte stream. Malformed headers are rejected rather
// than resynchronised by guessing a header inside arbitrary payload bytes.
class TcpFramer {
    uint8_t bytes_[MaxTcpAdu] = {};
    size_t size_ = 0;
public:
    enum class State { More, Complete, Invalid };
    void reset() { size_ = 0; }
    const uint8_t* data() const { return bytes_; }
    size_t size() const { return size_; }
    // Caller must process and reset on Complete before feeding the next ADU.
    State push(uint8_t byte) {
        if (size_ >= MaxTcpAdu || (size_ >= 6 && size_ >= size_t(6 + word(bytes_ + 4)))) return State::Invalid;
        bytes_[size_++] = byte;
        if (size_ >= 6) {
            const uint16_t length = word(bytes_ + 4);
            if (word(bytes_ + 2) != 0 || length < 2 || length > 254) return State::Invalid;
            if (size_ == size_t(6 + length)) return State::Complete;
        }
        return State::More;
    }
};

} // namespace wilcoxon883m
