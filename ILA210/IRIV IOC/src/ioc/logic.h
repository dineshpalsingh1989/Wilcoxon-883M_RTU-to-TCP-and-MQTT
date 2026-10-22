#pragma once
#include <stdint.h>

namespace ioc {
struct Delay {
    bool pending = false;
    uint32_t since = 0;
    bool update(bool condition, uint32_t now, uint32_t delay) {
        if (!condition) { pending = false; return false; }
        if (!pending) { pending = true; since = now; }
        return uint32_t(now - since) >= delay;
    }
};
struct Debounce {
    bool stable = false, candidate = false, initialized = false;
    uint32_t since = 0;
    bool update(bool value, uint32_t now) {
        if (!initialized) { stable = candidate = value; since = now; initialized = true; return false; }
        if (value != candidate) { candidate = value; since = now; }
        if (candidate != stable && uint32_t(now - since) >= 50) {
            stable = candidate;
            return stable; // One activation edge; a held input never repeats.
        }
        return false;
    }
};
inline float terminalVolts(uint16_t adc, float referenceMv) {
    return adc / 4095.0f * referenceMv / 1000.0f * 16.0f / 5.0f;
}
inline float terminalMilliamps(uint16_t adc, float referenceMv) {
    return terminalVolts(adc, referenceMv) / 248.0f * 1000.0f;
}
inline float engineeringValue(float input, bool currentMode, float minimum, float maximum) {
    return minimum + (input - (currentMode ? 4.0f : 0.0f)) /
                     (currentMode ? 16.0f : 10.0f) * (maximum - minimum);
}
struct Thresholds {
    bool low = false, high = false;
    bool update(float value, bool lowEnabled, float minimum, bool highEnabled,
                float maximum, float hysteresis) {
        low = lowEnabled && (low ? value < minimum + hysteresis : value < minimum);
        high = highEnabled && (high ? value > maximum - hysteresis : value > maximum);
        return low || high;
    }
};
struct AlarmLatch {
    uint32_t previous = 0;
    bool latched = false, acknowledged = false;
    void update(uint32_t sources, bool acknowledge) {
        if (sources & ~previous) acknowledged = false;
        if (sources) latched = true;
        if (acknowledge) acknowledged = true;
        if (!sources && acknowledged) latched = false;
        if (!latched) acknowledged = false;
        previous = sources;
    }
};
inline bool outputActive(uint8_t source, uint32_t alarms, const AlarmLatch& latch,
                         bool inputEnabled, bool inputActive) {
    return source == 1 ? alarms != 0 : source == 2 ? latch.latched :
           source == 3 ? latch.latched && !latch.acknowledged :
           source == 4 ? inputEnabled && inputActive : false;
}
}
