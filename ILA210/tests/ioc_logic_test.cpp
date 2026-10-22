#include "../src/ioc/logic.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
using namespace ioc;
int main() {
    Delay delay;
    assert(!delay.update(true, 0, 100)); // millis()==0 is a valid start time.
    assert(!delay.update(true, 99, 100));
    assert(delay.update(true, 100, 100));
    assert(!delay.update(false, 101, 100));
    assert(!delay.update(true, UINT32_MAX - 20, 50));
    assert(!delay.update(true, 28, 50));
    assert(delay.update(true, 29, 50)); // Delay survives timer rollover.
    assert(!delay.update(false, 30, 0));
    assert(delay.update(true, 31, 0));

    Debounce button;
    assert(!button.update(true, 0)); // Active at boot: no accidental capture.
    assert(!button.update(true, 1000));
    assert(!button.update(false, 1001));
    assert(!button.update(false, 1051));
    assert(!button.update(true, 1052));
    assert(!button.update(false, 1070)); // Contact bounce restarts debounce.
    assert(!button.update(true, 1080));
    assert(!button.update(true, 1129));
    assert(button.update(true, 1130));
    assert(!button.update(true, 2000)); // Held button fires only once.
    button = Debounce();
    assert(!button.update(false, UINT32_MAX - 100));
    assert(!button.update(true, UINT32_MAX - 10));
    assert(button.update(true, 39));

    Thresholds limits;
    assert(!limits.update(50, true, 10, true, 90, 2));
    assert(limits.update(91, true, 10, true, 90, 2));
    assert(limits.update(89, true, 10, true, 90, 2));
    assert(!limits.update(88, true, 10, true, 90, 2));
    assert(limits.update(9, true, 10, true, 90, 2));
    assert(limits.update(11, true, 10, true, 90, 2));
    assert(!limits.update(12, true, 10, true, 90, 2));
    assert(!limits.update(-100, false, 10, false, 90, 2));

    AlarmLatch alarm;
    alarm.update(1, false); assert(alarm.latched && !alarm.acknowledged);
    alarm.update(1, true); assert(alarm.latched && alarm.acknowledged);
    assert(outputActive(1, 1, alarm, false, false)); // Ack never bypasses an active alarm.
    assert(!outputActive(3, 1, alarm, false, false)); // Sounder is silenced.
    alarm.update(3, false); assert(!alarm.acknowledged); // A second source sounds again.
    assert(outputActive(3, 3, alarm, false, false));
    alarm.update(0, false); assert(alarm.latched); // Returned to normal; still needs ack.
    assert(outputActive(2, 0, alarm, false, false));
    assert(!outputActive(1, 0, alarm, false, false));
    alarm.update(0, true); assert(!alarm.latched);
    assert(!outputActive(0, 3, alarm, true, true)); // Disabled output stays off.
    assert(!outputActive(4, 0, alarm, false, true)); // Cannot follow a disabled input.
    assert(outputActive(4, 0, alarm, true, true));
    alarm.update(4, false); alarm.update(4, true); alarm.update(0, false);
    assert(!alarm.latched); // Ack while active clears the latch when condition clears.

    assert(fabsf(terminalVolts(4095, 3320) - 10.624f) < .001f);
    assert(fabsf(terminalMilliamps(4095, 3320) - 42.8387f) < .001f);
    assert(fabsf(engineeringValue(4, true, -50, 150) + 50) < .001f);
    assert(fabsf(engineeringValue(12, true, -50, 150) - 50) < .001f);
    assert(fabsf(engineeringValue(20, true, -50, 150) - 150) < .001f);
    assert(engineeringValue(0, true, 0, 100) == -25); // Faults aren't clamped into normal range.
    assert(engineeringValue(5, false, 0, 100) == 50);
    puts("PASS: IOC timing/rollover, debounce, hysteresis, acknowledgement/re-alarm, output ownership and analog scaling.");
}
