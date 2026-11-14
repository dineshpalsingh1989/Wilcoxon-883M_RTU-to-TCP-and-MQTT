# Sensor discovery, configuration and diagnostics

Available in both independent ILA210 board projects: ESP32-S3 and IRIV IOC.
Open **Settings → Modbus RS485 → Find sensor, change sensor settings and
diagnostics**, or visit `/sensor_setup`. The sensor-loss page also links here.
The legacy RS485 form still changes only the gateway connection settings.

## Discovery

Connect one sensor and select an ID range (default 1–16, maximum 1–247).
Search checks 9600, 19200, 38400, 57600 and 115200 baud, always 8N1.
One bounded probe runs per background pass with a 250 ms response timeout.
Progress shows completed probes; a full search can take several minutes.
Cancel restores the original gateway connection after the current probe.

Search uses FC03 to read holding registers 0–4, verifying the baud code,
eight data bits, no parity, one stop bit and responding ID against manual
97049 Rev.A1 page 14. It stops at the first compatible settings signature.
This is compatibility detection, not device identity verification. Devices
configured for other formats or unsupported baud rates will not be discovered.
Search never writes to the sensor and does not save a discovered connection.
Select **Use detected connection** to reverify and save the gateway settings.

## Sensor configuration

Verify the current connection first. Change **one setting per operation**:
either baud or ID. The backend verifies the current settings, then sends FC06
to holding register `0x0000` (baud code 9–13) or `0x0004` (ID 1–247).
It tests the new connection up to three times and saves the gateway settings
only when readback matches. No other sensor settings are written.

A missing write echo does not prove the write failed: changing serial settings
can interrupt that echo. Readback decides success. If it fails, the gateway
restores its original connection, reports uncertainty and does not issue
automatic rollback writes. The sensor may already have changed or may require
a restart. Search before retrying. Sensor persistence/restart behavior must be
verified on the actual hardware. A storage failure is explicitly reported;
the verified connection then remains active only until gateway restart.

Use this on a single-sensor connection. Choose IDs that do not conflict with
other devices before reconnecting a multidrop bus. Power loss between the
sensor write and gateway save can require discovery after restart.

## Scheduling, alarms and diagnostics

HTTP requests queue work; only the existing background task touches the UART.
Setup is rejected during capture or dynamic downloads. Existing MQTT waveform publication continues while setup runs.
Capture requests are rejected while setup is reserved. Normal vibration reads
pause during setup; MQTT servicing, TCP polling and the existing sensor-loss
policy continue. A long scan can therefore activate configured loss alarms.
Check that policy and commission with controlled loads. Cached PLC readings
remain stale until normal polling resumes; no new validity register is added.

The page shows RTU successes, timeouts, CRC errors, exception responses and
other errors; successful transaction counts include writes and setup probes.
It also shows the last valid vibration reading's age, MQTT connection state,
TCP client/request counts, malformed frames and TCP timeouts. Counters are
bounded integer fields and reset at reboot. Diagnostics snapshots are copied
under the existing mutex; the web task does not inspect live protocol buffers.
Successful connection changes invalidate metrics and restart the metric round,
so readings from different connections are not combined.

Settings/control pages retain the project's existing trusted-network access
model. These new routes do not add authentication.

## Preview and software verification

Run `node ILA210/demo/mock_server.js` and open
`http://localhost:8091/sensor_setup`. This renders the embedded page with
**simulated** search/configuration responses; no serial ports are opened.

Native protocol and setup tests: run `python tests/run_native.py` from either
board's `lib/WilcoxonILA210Modbus` directory. Setup tests cover bounded scans,
restoration, cancellation, verified selection, single-setting changes, failed
writes/storage and timer rollover. Run
`node ILA210/tests/sensor_setup_browser_test.js` for the simulated page checks.
Physical acceptance is tracked in the [bench checklist](hardware-bench.md).

Source builds contain these features. Previously published firmware release
assets are unchanged until a new firmware release is built and published.
