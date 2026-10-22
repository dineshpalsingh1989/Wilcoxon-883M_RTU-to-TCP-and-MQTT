#pragma once
#include <stdint.h>

namespace wilcoxonila210 {
// Imported ILA firmware mapping, preserved verbatim in meaning. The exact
// ILA200/ILA210 register manual is still required for hardware acceptance.
struct MetricRegister {
    const char* name;
    const char* unit;
    float scale; // Published TCP integer multiplier.
    uint16_t inputAddress;
    float readScale; // Existing signed int16 sensor conversion.
};
static const MetricRegister MetricRegisters[] = {
    {"Accel RMS 1", "g", 100, 0x0000, .01f},
    {"Accel Peak 1", "g", 100, 0x0004, .01f},
    {"Accel RMS 2", "g", 100, 0x0010, .01f},
    {"Accel Peak 2", "g", 100, 0x0014, .01f},
    {"Velo RMS", "mm/s", 10, 0x0020, .1f},
    {"Velo Peak", "mm/s", 10, 0x0024, .1f},
    {"Displ RMS", "um", 1, 0x0030, 1},
    {"Displ Peak", "um", 1, 0x0034, 1},
    {"True Peak", "g", 100, 0x0040, .01f},
    {"Crest Factor", "", 100, 0x0044, .01f},
    {"Std Deviation", "g", 100, 0x0048, .01f},
    {"Temperature", "C", 10, 0x0050, .1f},
    {"Drive Voltage", "V", 100, 0x0060, .01f},
    {"Board Temp", "C", 10, 0x007c, .1f}
};
constexpr uint8_t MetricCount = sizeof(MetricRegisters) / sizeof(MetricRegisters[0]);
constexpr uint8_t VelocityPeakIndex = 5;
constexpr uint16_t CaptureControl = 0x0200, CaptureRequest = 0xffff, CaptureReady = 0;
constexpr uint16_t WaveformStart = 0x1f01, WaveformPoints = 25600;
constexpr uint16_t SpectrumStart = 0x0600, SpectrumPoints = 6401;
}
