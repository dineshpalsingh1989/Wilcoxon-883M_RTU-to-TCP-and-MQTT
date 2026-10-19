#pragma once
#include "RtuProtocol.h"

namespace wilcoxon883m {
// Transport is owned by the caller; a client has exactly one task/owner.
// Required methods: ms(), us(), idle(), available(), read(), write(), flush(), tx().
template<class Transport> class Client {
    Transport& port_;
    uint8_t unit_ = 1;
    uint32_t silenceUs_ = 1750;
    bool configured_ = false;
    uint8_t exception_ = 0;
public:
    explicit Client(Transport& port) : port_(port) {}
    bool configure(uint32_t baud, uint8_t unit, uint8_t characterBits) {
        if (!baud || baud > 921600 || unit < 1 || unit > 247 || characterBits < 10 || characterBits > 12) return false;
        unit_ = unit;
        silenceUs_ = baud > 19200 ? 1750 : (3500000UL * characterBits + baud - 1) / baud;
        configured_ = true; return true;
    }
    void reset() { configured_ = false; exception_ = 0; }
    uint8_t exceptionCode() const { return exception_; }
    Result transact(uint8_t fn, uint16_t address, uint16_t value,
                    uint16_t* output, size_t capacity, uint32_t timeoutMs) {
        exception_ = 0;
        if (!configured_ || !timeoutMs || timeoutMs > 60000 ||
            (fn != 6 && (!output || capacity < value))) return Result::InvalidArgument;
        uint8_t request[8], response[256];
        if (!requestRtu(unit_, fn, address, value, request, sizeof(request))) return Result::InvalidArgument;
        uint32_t start = port_.ms(), lastByte = port_.us();
        // Drain old input and require a complete silent interval, bounded even
        // on a noisy bus. Timeout arithmetic remains safe across timer wrap.
        while (uint32_t(port_.us() - lastByte) < silenceUs_) {
            while (port_.available()) {
                port_.read(); lastByte = port_.us();
                if (uint32_t(port_.ms() - start) >= timeoutMs) return Result::Timeout;
            }
            if (uint32_t(port_.ms() - start) >= timeoutMs) return Result::Timeout;
            port_.idle();
        }
        port_.tx(true);
        size_t sent = port_.write(request, sizeof(request));
        port_.flush(); // Must finish transmitting before releasing DE.
        port_.tx(false);
        if (sent != sizeof(request)) return Result::Transport;
        start = port_.ms(); lastByte = port_.us(); size_t used = 0;
        while (uint32_t(port_.ms() - start) < timeoutMs) {
            if (port_.available()) {
                int byte = port_.read();
                if (byte < 0) continue;
                if (used == sizeof(response)) return Result::Frame;
                response[used++] = uint8_t(byte); lastByte = port_.us();
            } else {
                if (used && uint32_t(port_.us() - lastByte) >= silenceUs_)
                    return decodeRtu(response, used, unit_, fn, address, value, output, capacity, exception_);
                port_.idle();
            }
        }
        return Result::Timeout;
    }
    Result readInput(uint16_t address, uint16_t count, uint16_t* out, size_t capacity, uint32_t timeout = 2000) {
        return transact(4, address, count, out, capacity, timeout);
    }
    Result readHolding(uint16_t address, uint16_t count, uint16_t* out, size_t capacity, uint32_t timeout = 2000) {
        return transact(3, address, count, out, capacity, timeout);
    }
    Result writeSingle(uint16_t address, uint16_t value, uint32_t timeout = 2000) {
        return transact(6, address, value, nullptr, 0, timeout);
    }
};
struct Metrics {
    float accelerationRmsG[3];
    float velocityRmsMmS[3];
    float temperatureC;
};
// Manual Rev.1.0 sections 6/7: metric vibration magnitudes are unsigned;
// temperature at 0x0050 is signed. Do not reinterpret all registers as int16.
template<class Rtu> Result readMetrics(Rtu& rtu, Metrics& output, uint32_t timeout) {
    uint16_t a[3], v[3], t;
    Result status = rtu.readInput(0x0000, 3, a, 3, timeout);
    if (status != Result::Ok) return status;
    status = rtu.readInput(0x0020, 3, v, 3, timeout);
    if (status != Result::Ok) return status;
    status = rtu.readInput(0x0050, 1, &t, 1, timeout);
    if (status != Result::Ok) return status;
    Metrics next;
    for (unsigned i = 0; i < 3; ++i) {
        next.accelerationRmsG[i] = a[i] * 0.01f;
        next.velocityRmsMmS[i] = v[i] * 0.1f;
    }
    const int32_t signedTemperature = t < 0x8000 ? int32_t(t) : int32_t(t) - 65536;
    next.temperatureC = signedTemperature * 0.1f;
    output = next; return Result::Ok;
}
} // namespace wilcoxon883m
