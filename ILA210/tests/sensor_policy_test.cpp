#include "../IRIV IOC/src/core/sensor_policy.h"
#include <assert.h>
#include <stdio.h>
using namespace gateway;
int main() {
    assert(!commandTargetsGateway(nullptr, "board-a"));
    assert(!commandTargetsGateway("", "board-a"));
    assert(!commandTargetsGateway("00000006", "board-a"));
    assert(!commandTargetsGateway("board-a", "board-b"));
    assert(commandTargetsGateway("board-a", "board-a"));
    SensorHealth health; health.startedAt = 100;
    assert(!health.lost(10099)); assert(health.lost(10100));
    health.seen = true; health.lastSuccessAt = 11000;
    assert(!health.lost(11000)); assert(health.lost(21000));
    health.lastSuccessAt = UINT32_MAX - 5000;
    assert(!health.lost(4998)); assert(health.lost(4999));
    health.lastSuccessAt = 5000; assert(!health.lost(5000)); // Recovery clears raw fault.
    assert(!forceAlarmOnLoss(0, true)); assert(!forceAlarmOnLoss(1, false)); assert(forceAlarmOnLoss(1, true));
    SensorSchedule schedule;
    assert(schedule.next(0, true) == RtuJob::Alarm);
    schedule.complete(RtuJob::Alarm, 0);
    // Full metrics progress even during capture, but never defer an alarm deadline.
    for (int i = 0; i < 14; ++i) {
        assert(schedule.next(10+i*10, true) == RtuJob::Metrics);
        assert(schedule.metricIndex() == i);
        assert(schedule.complete(RtuJob::Metrics, 10+i*10) == (i == 13));
    }
    assert(schedule.next(200, true) == RtuJob::Capture);
    assert(schedule.next(500, true) == RtuJob::Alarm);
    schedule.complete(RtuJob::Alarm, 500);
    assert(schedule.next(510, true) == RtuJob::Capture);
    assert(schedule.next(510, false) == RtuJob::None);
    assert(schedule.next(2010, true) == RtuJob::Alarm);
    schedule.complete(RtuJob::Alarm, 2010);
    assert(schedule.next(2140, true) == RtuJob::Metrics);
    assert(schedule.complete(RtuJob::Metrics, 2141, false));
    assert(schedule.next(2142, true) == RtuJob::Capture); // Failed batch backs off immediately.
    SensorSchedule slow;
    slow.complete(RtuJob::Alarm, 0);
    for (int i=0; i<14; ++i) {
        const uint32_t now = 1+i*300;
        if (slow.next(now, true)==RtuJob::Alarm) slow.complete(RtuJob::Alarm, now);
        assert(slow.next(now,true)==RtuJob::Metrics);
        slow.complete(RtuJob::Metrics,now+250);
    }
    slow.complete(RtuJob::Alarm,4200);
    assert(slow.next(4201,true)==RtuJob::Capture); // Slow metrics cannot monopolize RTU.
    SensorSchedule rollover;
    rollover.complete(RtuJob::Alarm, UINT32_MAX - 200);
    assert(rollover.next(298, true) != RtuJob::Alarm);
    assert(rollover.next(299, true) == RtuJob::Alarm);
    puts("PASS: gateway targeting, sensor-loss timeout/recovery/rollover and alarm-priority RTU scheduling during captures.");
}
