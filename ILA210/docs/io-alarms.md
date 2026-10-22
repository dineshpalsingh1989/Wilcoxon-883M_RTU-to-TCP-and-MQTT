# IOC I/O and alarms

Open **Settings > IOC I/O & Alarms** on an IRIV build. The sensor dashboard stays separate. All new input actions, analog channels and spare outputs start disabled.

| Page | Purpose |
| --- | --- |
| `/ioc` | Live channel states, alarm summary, capture acceptance counts and acknowledgement |
| `/ioc/inputs` | Name DI0?DI10; choose monitoring, delayed alarm, capture push-button or acknowledge push-button |
| `/ioc/analog` | Configure AN0/AN1 as 4?20 mA or 0?10 V inputs, engineering scaling and low/high alarms |
| `/ioc/outputs` | Assign DO2/DO3 to active alarm, latched beacon, unacknowledged sounder or a monitored DI |

The ESP32-S3 build reports that this hardware is unavailable. Its existing external relay and sensor settings remain separate.

## Useful setup examples

**Capture and acknowledge buttons:** on Inputs & push-buttons, choose the DI0 capture / DI1 acknowledge preset. Select the correct active polarity for each wired input, then save. The firmware debounces for 50 ms and fires once per activation. A button already active at boot or after configuration changes must be released before it can fire. Busy captures are rejected and counted; they are not silently queued. These are sampled push-buttons, not high-speed pulse counters.

**Pressure transmitter:** on Analog & 4?20 mA, name AN0 `Pressure`, select 4?20 mA, set the unit to `bar`, the engineering range to 0?10, enable a high alarm at 8, choose 0.5 hysteresis and a 1000 ms delay. The high alarm starts above 8 bar after the delay and resets at or below 7.5 bar. Below 3.6 mA or above 21 mA is shown as a signal fault; the separate fault-alarm option controls whether that fault activates outputs. Check the transmitter's actual diagnostic-current specification.

**Beacon and sounder:** on Alarm outputs, choose the DO2 latched beacon / DO3 sounder preset and save. A new alarm source activates both. Acknowledge silences DO3, while DO2 stays active until all conditions clear. A second source arriving after acknowledgement sounds DO3 again. If an alarm clears before acknowledgement, the beacon/sounder remain latched until acknowledged. Active-alarm mode instead follows the current combined condition without latching.

Combined alarms include enabled digital and analog alarms, active vibration alert/alarm outputs, and sensor communication loss when enabled. Configure the loss timeout and output policy on the separate [sensor communication page](sensor-communication.md) at `/sensor_fault_settings`. The raw fault is visible even when excluded from the combined alarm. Configure those vibration thresholds at `/relay_settings`. DO0 and DO1 are reserved for that existing code; the I/O sampler never writes them. A follow-input output requires that DI to be enabled, such as Monitor machine signal.

## Wiring and calibration

The IOC has **analog inputs**, not a 4?20 mA analog output. Its physical AN-IN mode switch must match Current or Voltage; software does not change that switch. Follow the [manufacturer's board resources](https://my.cytron.io/p-iriv-io-controller) for terminal wiring, load limits and power, and the [functional wiring diagrams](wiring.md). Do not connect an unknown signal before checking its mode and range.

Conversions follow [Cytron's analog example](https://github.com/CytronTechnologies/Cytron-IRIV-IO-Controller/blob/main/examples/circuitpython/analog_input/analog_input.py): 16/5 divider ratio, 248-ohm current conversion, and a default reference of 3320 mV. This firmware explicitly samples the ADC at 12 bits and averages eight readings. Voltage is `ADC/4095 ? reference_voltage ? 16/5`; current in mA is `voltage/248 ? 1000`. Calibrate against known signals before relying on engineering thresholds. Engineering readings are not clamped, so a broken current loop remains visible outside the 4?20 mA range. Near full-scale ADC readings are also marked as faults.

## Sampling, storage and behavior

A dedicated 8 KiB FreeRTOS task polls digital inputs about every 20 ms and analog channels about every 100 ms on IRIV. Digital debounce, alarm delays and hysteresis run independently of RTU reads and MQTT reconnects. These are scheduling targets, not measured or guaranteed response times. Flash writes and other work can add jitter. This is ordinary monitoring/alarm firmware, not a safety interlock.

Validated configuration is stored atomically in LittleFS `/ioc.json` using a temporary file and rename. Invalid or absent stored configuration falls back to disabled channels/outputs. Runtime latches and capture counts are not retained across reboot. Outputs start low and their saved functions apply once sampling begins. Disabling an alarm source does not acknowledge an existing latch; acknowledge after reviewing the change. Choose output behavior and check power-loss/communication-loss requirements on the actual installation.

Pages use fresh status before enabling save/acknowledge controls; on a network failure they show unknown states. A configuration revision prevents an old browser tab overwriting newer saved settings. HTTP controls retain the gateway's trusted-network requirement; authentication is not provided.

## Integration

- `GET /api/ioc`: supported flag, validated configuration, sampled state, freshness/age, alarm conditions, output states and capture counters.
- `POST /api/ioc/config`: complete JSON configuration with its current `revision`; invalid input returns 400, outdated revisions return 409, persistence errors return 503. No changes are applied on failed persistence.
- `POST /api/ioc/acknowledge`: requests acknowledgement in the sampling task.
- MQTT `<configured base topic>/io`: non-retained `ioc_status` messages on digital/alarm/output transitions and roughly every 10 seconds, including `device_id`, `fresh`, `age_ms`, analog values and input/output states. The network task owns publishing; the sampler does not touch the MQTT client. QoS 0 messages may be lost, and offline events are not replayed. Existing topics and Modbus register mappings are unchanged.

## Software checks and bench validation

`tests/ioc_logic_test.cpp` exercises the actual timing, debounce, hysteresis, latch/output and conversion helpers, including timer rollover and a second alarm after acknowledgement. Compile it with a native C++11 compiler and run the resulting executable. The workspace has a Zig compiler under `.tools/native-compiler/ziglang/zig.exe`; use `c++ -nostdlib++ -fno-exceptions -fno-rtti -std=c++11` with that compiler.

Run `node ILA210/tests/ioc_browser_test.js` from the parent workspace for separate-page navigation, configuration preservation, failed-save recovery, acknowledgement, mobile/focus, offline recovery and injection checks using simulated HTTP responses. These tests cannot validate physical I/O.

Before use, bench-test input polarity, contact bounce, a held button at startup, simultaneous alarm sources, real 4/12/20 mA and 0/5/10 V values, broken-loop indication, hysteresis and delay, external beacon/sounder loads, rebooted output behavior, SD/network traffic, and an unplugged RTU sensor while operating buttons. No hardware validation has been performed.

Initial verification on 2026-10-22 for `1.5.0-dev` (see the [readiness report](ioc-readiness.md) for the current fixed build): native IOC logic tests, simulated IOC browser tests, existing dashboard browser tests, local documentation links and `git diff --check` passed. Both PlatformIO targets compiled successfully:

| Target | Static RAM | Application flash |
| --- | --- | --- |
| ESP32-S3 | 122,300 / 327,680 bytes | 1,127,349 / 3,145,728 bytes |
| IRIV RP2350 | 203,960 / 524,288 bytes | 367,284 / 1,044,480 bytes |

Static memory figures do not measure runtime heap, task-stack headroom or response time. The IOC sampler adds a statically allocated 8 KiB task stack on IRIV only.
