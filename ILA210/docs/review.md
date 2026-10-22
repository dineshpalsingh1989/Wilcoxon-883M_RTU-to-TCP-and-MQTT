# ILA firmware review

Reviewed on 2026-10-22. Changes are local to the ILA repository. No device was flashed.

The IOC I/O extension is documented separately in [io-alarms.md](io-alarms.md), with dedicated sampling, separate configuration pages and native/browser regression checks.

## Corrected defects

| Area | Defect and correction |
| --- | --- |
| Board support | Added standalone platform support for IRIV RP2350 Ethernet, LittleFS settings, isolated UART1, DO0/DO1 and the monochrome LED. SD initialization and Wi-Fi configuration are disabled on IRIV. |
| Build | Added pinned PlatformIO dependencies and a normal C++ entry point. Fixed the baseline aggregate assignment that failed under the Arduino C++ standard. |
| Spectrum history | Replaced raw `memcpy` of structs containing Arduino `String` objects with normal assignment, preventing dangling/shared string storage. |
| RTU capture | Reduced each request from 125 registers to the library's 64-register response capacity. No sensor register addresses were changed. |
| Locking | Event logging no longer reacquires an already-held nonrecursive mutex. Filesystem work, relay MQTT alerts and internal CSV network transfers no longer hold the data mutex. |
| SD concurrency | Serialized file writes, deletes, listing and downloads with a separate mutex. Added bounded CSV rotation and proper CSV escaping for event text. |
| Capture | Reject duplicate requests during pending/active capture or a streamed response. Keep capture buffers stable during streaming; terminate a failed stream instead of inventing zero samples. Finish chunked HTTP responses. |
| MQTT waveform | Replaced the 51 KB full-waveform heap copy and blocking publication loop with one bounded chunk per task pass. Aborted transfers are logged. |
| MQTT packets | Corrected an overload that treated payload length as a retained flag. Added JSON bounds checks and smaller chunks/preview so documents fit. |
| MQTT connection | Added device-specific client IDs and derived command/status topics from the configured base topic. Background task owns the MQTT client; web status uses an atomic snapshot. |
| TLS | Removed unconditional `setInsecure()`. TLS requires a locally configured root CA; port 1883 explicitly selects plain MQTT. |
| Settings | Validate interval, serial address/baud, MQTT topic/port, TCP port and relay thresholds/delay before saving. Restore safe numeric defaults for malformed saved values. Avoid a shared Preferences handle across task cores. |
| Credentials/UI | Send Wi-Fi and MQTT credentials with POST, escape broker text in HTML and keep the stored broker password out of page HTML. Render Wi-Fi SSIDs as text. |
| Time | Use UTC for timestamps ending in `Z`; prevent unsynchronized clock reads from blocking for five seconds. |
| Analysis | Compute variance from squared deviations to avoid negative variance from rounding. Reject empty baselines and invalidate baseline work after recording restart/clear. |
| Relays/tasks | Reset interrupted threshold dwell on read failure; disabled relays are serviced even when the sensor stops answering. Check task and MQTT buffer allocation results. |
| Metric validity | Hide startup/failed-read values and report the age of the last successful read instead of labeling each browser poll as a sensor update. TCP values retain the last cached readings; no new validity register was invented. |
| Dashboard | Use local canvas charts, finite-value checks, explicit capture state, failed-download recovery and offline/reconnect feedback. Correct temperature/voltage units and mobile analysis-table overflow. |

## Verification

- PlatformIO builds the ESP32-S3 and IRIV RP2350 targets; build output gives static RAM and flash usage. Static RAM does not measure runtime TLS, JSON, network or task-stack headroom.
- `node tests/browser_test.js` runs the real embedded HTML/scripts against a simulated HTTP server. It covers desktop/mobile tabs, local charts without external requests, waveform loading, capture busy state, failed download/retry, offline/recovery, keyboard focus, SSID injection and blank broker password fields.
- Source/diff checks cover whitespace and local documentation links.

Baseline build results before the IOC I/O extension (`1.4.1-dev`):

| Target | Static RAM | Application flash | Result |
| --- | --- | --- | --- |
| ESP32-S3 | 122,300 / 327,680 bytes | 1,109,681 / 3,145,728 bytes | PASS |
| IRIV RP2350 | 194,964 / 524,288 bytes | 338,036 / 1,044,480 bytes | PASS |

The workspace toolchain initially failed to launch a compiler subprocess. The successful builds used the existing user-level packages, with these PowerShell settings from the parent workspace:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path (Get-Location) '.pio-core'
$env:PLATFORMIO_PACKAGES_DIR = 'C:/Users/user/.platformio/packages'
.venv/Scripts/python.exe -m platformio run -d ILA210 -e esp32s3 -e cytron_iriv_io_controller -j 2
node ILA210/tests/browser_test.js
git -C ILA210 diff --check
```

The parent project's `node demo/smoke_test.js` also passed. It exercises the 883M demo, rather than ILA hardware. Both builds and simulated browser checks passed; actual sensor/board validation remains outstanding below.

## Outstanding hardware and integration checks

1. **Sensor identity/manual:** confirm ILA200 versus ILA210. No ILA manual was provided. The 883M manual cannot establish ILA register correctness. Verify function codes, register offsets, signedness, scaling, capture trigger/ready semantics, sample counts and frequency resolution against the correct manual.
2. **Board and wiring:** ESP32-S3 and Cytron IRIV are the requested targets; confirm the exact ESP32-S3 model and actual SPI pins. The original README's SD clock pin conflicts with RS485 TX. Explicit pin constants have been preserved. Check relay active levels, power-up behavior, sensor voltage and RS485 direction timing on the bench.
3. **Sensor loss and relays:** priority velocity alarm reads continue during captures (500 ms scheduling target, not a response guarantee). `/sensor_fault_settings` configures timeout, the IOC communication-fault source, and DO0/DO1 hold/on/off behavior. Default timeout is 10 seconds; default output policy holds last state. Bench-test all policies, sensor disconnection during a capture, and recovery against installation requirements.
4. **Network security:** HTTP settings/controls have no authentication or HTTPS. Credential POST prevents URL leakage, but transport remains plaintext. Several noncredential state-changing routes still use GET. Use a trusted isolated network; adding authentication needs a provisioning/recovery design.
5. **Failure/recovery:** test RTU CRC/timeouts/disconnects mid-capture, reconnect, concurrent browser downloads, SD removal/full-card/rotation failures, broker interruption and incomplete QoS 0 transfers. SD busy timeouts can skip log entries; retention is bounded, not lossless.
6. **Performance:** measure free heap/minimum heap, task stack high-water marks, watchdog behavior and multi-client responsiveness with real TLS and full captures. MQTT connection and TLS handshakes remain synchronous but have reduced timeouts. The platform layer adds a 60-second background-task watchdog. Bench-test reset/recovery and task-stack high-water marks on both boards; compile success does not exercise the watchdog.
7. **Analysis:** bench-test peak matching when nearby frequencies exchange rank, missing peaks and machine baseline changes. The statistical detector monitors matched peaks; it is not a fault classifier and does not flag every newly appearing frequency.
8. **Integration:** test actual ESP32-S3 broker CA rejection/acceptance, IRIV plain MQTT, DHCP/fallback/button recovery, custom MQTT topics, persistence across restart, negative-temperature TCP representation, and simultaneous PLC access. MQTT commands now require the unique hardware-derived `gateway_id`; migrate command publishers from `serial_number`. The unverified sensor serial is empty. Test two gateways on the same broker/topic. The imported TCP holding-register cache is not write-protected; clients must treat it as read-only.

No hardware, electrical, RTU protocol or production-readiness claims follow from the software checks.
