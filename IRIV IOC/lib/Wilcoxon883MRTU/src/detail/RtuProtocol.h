#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Portable RTU protocol subset; no Arduino or third-party Modbus dependencies.
namespace wilcoxon883m {

constexpr uint16_t MaxRead = 125;
inline uint16_t word(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
inline void put(uint8_t* p, uint16_t value) { p[0] = value >> 8; p[1] = value & 255; }
inline bool range(uint16_t address, uint16_t count) {
    return count && uint32_t(address) + count <= 65536UL;
}
inline uint16_t crc16(const uint8_t* bytes, size_t size) {
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    return crc;
}
inline bool validCrc(const uint8_t* bytes, size_t size) {
    return size >= 4 && crc16(bytes, size - 2) ==
        (uint16_t(bytes[size - 2]) | uint16_t(bytes[size - 1]) << 8);
}
inline size_t requestRtu(uint8_t unit, uint8_t function, uint16_t address,
                         uint16_t value, uint8_t* out, size_t capacity) {
    if (!out || capacity < 8 || unit < 1 || unit > 247) return 0;
    if (function != 3 && function != 4 && function != 6) return 0;
    if (function != 6 && (value > MaxRead || !range(address, value))) return 0;
    out[0] = unit; out[1] = function; put(out + 2, address); put(out + 4, value);
    uint16_t crc = crc16(out, 6); out[6] = crc & 255; out[7] = crc >> 8;
    return 8;
}
enum class Result { Ok, InvalidArgument, Timeout, Frame, Crc, Exception, Transport, Busy };
// Validate the entire response before modifying the caller's output.
inline Result decodeRtu(const uint8_t* frame, size_t size, uint8_t unit,
                        uint8_t function, uint16_t address, uint16_t value,
                        uint16_t* output, size_t capacity, uint8_t& exception) {
    exception = 0;
    if (unit < 1 || unit > 247 || (function != 3 && function != 4 && function != 6)) return Result::InvalidArgument;
    if (function != 6 && (!range(address, value) || value > MaxRead || !output || capacity < value)) return Result::InvalidArgument;
    if (!frame || size < 5 || size > 256) return Result::Frame;
    if (!validCrc(frame, size)) return Result::Crc;
    if (frame[0] != unit) return Result::Frame;
    if (frame[1] == uint8_t(function | 0x80)) {
        if (size != 5 || frame[2] == 0) return Result::Frame;
        exception = frame[2]; return Result::Exception;
    }
    if (frame[1] != function) return Result::Frame;
    if (function == 6) {
        return size == 8 && word(frame + 2) == address && word(frame + 4) == value ? Result::Ok : Result::Frame;
    }
    if (size != size_t(5 + 2 * value) || frame[2] != 2 * value) return Result::Frame;
    for (size_t i = 0; i < value; ++i) output[i] = word(frame + 3 + 2 * i);
    return Result::Ok;
}

} // namespace wilcoxon883m
