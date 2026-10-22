# IOC deployment readiness

Updated on 2026-10-22 after the four review findings were addressed in source. The review and fixes concern the ILA project. No board was flashed, reset, opened over serial or driven.

**Verdict: all four reported software findings are fixed; final builds and software checks passed for `1.5.1-dev`. Hardware operation remains unverified.** A working browser demo does not establish that the IOC boots, reads the sensor correctly or safely drives connected equipment.

## Corrections

| Finding | Implemented behavior | Evidence and remaining limits |
| --- | --- | --- |
| Captures suspended vibration alarm evaluation | The RTU scheduler prioritizes Velocity Peak alarm reads on a 500 ms schedule, services individual metric reads and interleaves waveform/spectrum chunks. Relay evaluation no longer waits for a complete capture. | Native scheduling tests cover alarm priority during capture, metric progress, failure backoff and timer rollover. The 500 ms setting is a scheduling target, not a guaranteed response time: serial transactions, timeouts, network work and task delays can extend it. Measure end-to-end latency at the intended baud rate. |
| Failed/partial settings writes could report success | `GatewayPreferences` stages a complete namespace and requires explicit `commit()`. LittleFS commits use bounded JSON, a complete temporary write, exact readback and atomic rename. ESP32 commits store the namespace as one NVS string whose native write checks `nvs_commit`. HTTP handlers report 503 and avoid runtime changes/reboot after failed persistence. | Native tests compile the real wrapper against failure-injection backends for both platforms. They cover short writes, readback corruption, rename/lock/NVS failure, discard without commit, read-only access, overflow and repeated edits. Physical flash/power-cut behavior remains a bench check. |
| Multiple gateways accepted the same serial-addressed MQTT commands | Commands now require the unique hardware-derived `gateway_id`; outgoing identity and the sensor communication page expose that ID. The imported sensor serial remains explicitly unverified. | Native tests reject absent, empty, sensor-serial and different-gateway targets. Integrators must update command producers to send `gateway_id`. The correct sensor manual is still needed to implement a real sensor identity read. |
| Sensor loss had no communication-fault alarm/policy | A separate sensor communication page configures a 2,000–600,000 ms timeout, inclusion in IOC combined alarms and DO0/DO1 loss behavior: hold, force enabled DO1 alarm ON while holding DO0, or turn both OFF. Priority alarm samples refresh health; recovery returns output control to vibration thresholds. | Native tests cover freshness, startup timeout, recovery and rollover. Browser tests cover the actual embedded settings page with a simulated API. Physical loss/recovery behavior and the installation's chosen policy remain unverified. |

Relevant source:

- [RTU scheduling, health and command-target helpers](../IRIV%20IOC/src/core/sensor_policy.h)
- [Firmware integration and settings handlers](../IRIV%20IOC/src/main.cpp)
- [Transactional preferences](../IRIV%20IOC/src/platform/fs_preferences.h) and [storage implementation](../IRIV%20IOC/src/platform/fs_preferences.cpp)
- [Sensor communication page](../IRIV%20IOC/src/web/sensor_health_page.h)
- [IOC alarm integration](../IRIV%20IOC/src/ioc/ioc.cpp)

The separate page is available at `/sensor_fault_settings`; it reads and saves `/api/sensor_health`. The dashboard and existing IOC pages remain separate.

## Persistence and compatibility

IRIV keeps existing `/prefs/<namespace>.json` paths. ESP32 reads legacy per-key NVS values when no `__snapshot` exists and migrates complete namespaces on a successful save. A blank MQTT password preserves the existing saved password during migration. `end()` discards uncommitted staged changes.

Each namespace has a 2,048-byte JSON pool and a 1,536-byte serialized limit. Replacing strings compacts the candidate first, so repeated edits do not accumulate retired strings. Overflow or persistence failure leaves the previously saved snapshot in place; it does not authorize a successful HTTP response. Invalid existing JSON is rejected rather than silently overwritten with defaults by a save operation.

The native failure backends simulate storage API outcomes. They do not establish physical NVS or LittleFS behavior under brownout, power loss or flash wear.

## Verification

| Check | Current evidence |
| --- | --- |
| Native IOC logic suite | PASS in the review session; timing/rollover, debounce, hysteresis, latch/re-alarm, output selection and analog conversion helpers |
| Native sensor policy suite | PASS; command targeting, loss/recovery/rollover, alarm priority during capture and capture progress after slow successful metric rounds |
| Native ESP32 preferences wrapper suite | PASS; transaction/discard, legacy password migration, one-snapshot commit, 50 repeated long-string edits, read-only, overflow, invalid JSON and injected NVS failure |
| Native IRIV preferences wrapper suite | PASS; existing JSON compatibility, transaction/discard, partial-write, corrupt-readback, rename/lock failures and old-file retention |
| Sensor communication browser suite | PASS; actual embedded HTML with simulated responses, desktop/mobile, initial fetch, keyboard save, HTTP503, offline load/save, failure preserving edits, local assets and no horizontal overflow |
| Existing dashboard, IOC and local-demo browser suites | PASS after integration; desktop/mobile, captures, failure recovery and actual local demo sensor-loss timeout/output/recovery |
| ESP32-S3 and IRIV builds after these fixes | PASS for both final targets |
| UF2 family, payload and filesystem bounds after these fixes | PASS: 1,516 RP2350 ARM-S blocks (family `0xe48bff59`), payload `0x10000000..0x1005ec00`, below LittleFS at `0x100ff000` |
| Physical IOC and sensor checks | NOT PERFORMED |

Browser command from the parent workspace:

```powershell
node ILA210/tests/sensor_health_browser_test.js
```

Native persistence tests compile the actual `fs_preferences.cpp` with ArduinoJson 6.21.5 and the narrow test backends under `tests/preferences_stubs/`. Compiler options are `-fno-exceptions -fno-rtti -std=c++11 -Wall -Wextra -Werror`, with include paths to those stubs, `IRIV IOC/src/` and the installed ArduinoJson `src/`. Compile [preferences_test.cpp](../tests/preferences_test.cpp) for the NVS branch; compile [preferences_fs_test.cpp](../tests/preferences_fs_test.cpp) with `-DARDUINO_ARCH_RP2040` for LittleFS. These tests use the C++ runtime and do not use `-nostdlib++`.

Firmware build command, with the existing workspace toolchain:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path (Get-Location) '.pio-core'
$env:PLATFORMIO_PACKAGES_DIR = 'C:/Users/user/.platformio/packages'
.venv/Scripts/python.exe -m platformio run -d ILA210 -e esp32s3 -e cytron_iriv_io_controller -j 2
```

Recorded pre-split build sizes:

| Target | Static RAM | Application flash |
| --- | --- | --- |
| ESP32-S3 | 124,524 / 327,680 bytes | 1,142,729 / 3,145,728 bytes |
| IRIV RP2350 | 205,100 / 524,288 bytes | 375,612 / 1,044,480 bytes |

Recorded pre-split IOC artifact: `.pio/build/cytron_iriv_io_controller/firmware.uf2`. SHA-256: `d9e7797bf331ff8fe182adcad973d971ce46fe797185565886b2064875568f3e`. This is a built artifact only; no physical upload was performed. Static sizes do not measure runtime heap or task-stack headroom.

Browser checks: `browser_test.js`, `ioc_browser_test.js`, `sensor_health_browser_test.js` and `demo_browser_test.js` under `tests/` passed. Relative documentation links and `git diff --check` passed.

## Platform review and unresolved evidence

- The pinned Cytron variant matches UART, W5500, DI/DO and analog GPIO constants. IOC sampling owns DO2/DO3; vibration and sensor-loss policy own DO0/DO1.
- Installed Arduino-Pico starts `setup()` in a CORE0 FreeRTOS task after the scheduler starts. LittleFS initialization is not inherently a pre-scheduler fault.
- Application network tasks and Ethernet initialization run on core 0. LittleFS uses the core's FreeRTOS peer-idling mechanism for flash writes. No unconditional boot fault or flash-context deadlock was established by source review.
- Installed Arduino-Pico documentation requires LittleFS operations from one task. Current file operations across setup, web and background tasks use an external mutex. Inspection found no intrinsic task-owner check or reproduced crash; this remains a documented support caveat. A storage-owner task would comply with the documented contract.
- The watchdog establishes background-task progress, not independent web/sampler health. Measure runtime heap, stack high-water marks, reset recovery and real output timing.
- **The exact ILA200/ILA210 model and register manual remain missing.** The 883M manual cannot validate ILA register addresses, function codes, signedness, scaling, capture readiness or sample counts.
- HTTP controls have no authentication and need a trusted network. IRIV MQTT remains plain transport on port 1883.

## Pending bench acceptance

The following are pending physical actions. Uploading, resetting, interrupting power or driving outputs requires an explicit hardware instruction.

1. Disconnect machine loads; verify IOC power, sensor supply, RS485 wiring and physical analog mode switches against manufacturer documents.
2. The final UF2 family and bounds passed software inspection. Upload that exact Cytron UF2 and record its hash and boot log. Do not substitute the ESP32 `.bin`.
3. Confirm stable boot, Ethernet DHCP/fallback, USER-button fallback and page access.
4. Save every namespace, restart and confirm values, including migrated credentials. Exercise controlled storage failures and power interruption only with an authorized bench procedure.
5. Apply known 4/12/20 mA and 0/5/10 V sources. Verify calibration, scaling, limits, hysteresis, delay and broken-loop indication.
6. Check held-at-boot/released/pressed DI actions, capture rejection when busy, acknowledgement, simultaneous sources and re-alarm with benign indicator loads.
7. Select each sensor-loss output policy deliberately. Disconnect/reconnect the sensor and verify timeout, combined fault indication and DO0–DO3 behavior at startup and recovery.
8. With the correct ILA manual available, validate real readings/capture data and measure vibration alarm response during full captures at every intended baud rate.
9. Test actual MQTT gateway targeting with two devices, broker/PLC/browser coexistence, sustained captures, stack/heap headroom and controlled watchdog recovery.

Connected alarm equipment requires successful installation-specific hardware acceptance; software tests alone do not supply it.

## Reuse the review agent

Ask: **Use ioc_readiness_review to audit ILA210 for an IRIV bench upload. Do not flash hardware.** Definitions exist in the parent workspace and standalone ILA project. If the current session does not list the saved role, a delegated agent can follow its instructions or a new project session can load it.
