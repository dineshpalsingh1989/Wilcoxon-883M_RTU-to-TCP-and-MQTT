#pragma once
#include <stdint.h>
#include <string.h>

namespace gateway {
struct SensorHealth {
    uint32_t startedAt = 0, lastSuccessAt = 0, timeoutMs = 10000;
    bool seen = false, faultAlarmEnabled = true;
    bool lost(uint32_t now) const {
        return uint32_t(now - (seen ? lastSuccessAt : startedAt)) >= timeoutMs;
    }
};
inline bool commandTargetsGateway(const char* target, const char* identity) {
    return target && identity && *target && *identity && strcmp(target, identity) == 0;
}
enum class RtuJob { None, Alarm, Metrics, Capture };
// One sensor transaction per pass. Alarm reads have priority throughout a capture.
class SensorSchedule {
public:
    RtuJob next(uint32_t now, bool captureActive) {
        if (!alarmStarted_ || uint32_t(now - alarmAt_) >= 500) return RtuJob::Alarm;
        if (!metricsActive_ && (!metricsStarted_ || uint32_t(now - metricsAt_) >= 2000)) {
            metricsActive_ = metricsStarted_ = true; metricsAt_ = now; index_ = 0;
        }
        if (metricsActive_) return RtuJob::Metrics;
        return captureActive ? RtuJob::Capture : RtuJob::None;
    }
    uint8_t metricIndex() const { return index_; }
    // Returns true at the end of a full metric round or on its first failure.
    bool complete(RtuJob job, uint32_t now, bool success = true) {
        if (job == RtuJob::Alarm) { alarmStarted_ = true; alarmAt_ = now; }
        if (job != RtuJob::Metrics) return false;
        if (!success || ++index_ == 14) {
            metricsActive_ = false;
            metricsAt_ = now; // Leave capture time even after a slow successful round.
            return true;
        }
        return false;
    }
private:
    uint32_t alarmAt_ = 0, metricsAt_ = 0;
    uint8_t index_ = 0;
    bool alarmStarted_ = false, metricsStarted_ = false, metricsActive_ = false;
};
inline bool forceAlarmOnLoss(uint8_t mode, bool enabled) { return mode == 1 && enabled; }
}
