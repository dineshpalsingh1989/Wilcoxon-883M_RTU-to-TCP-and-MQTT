# Modbus library migration (beta)

From gateway firmware **v4.10.0-beta.1**, the gateway uses this repository's own [Wilcoxon883MRTU library](../experiments/Wilcoxon883MRTU-beta/README.md) for both Modbus sides. Stable releases up to v4.9.0 use `4-20ma/ModbusMaster` and `emelianov/modbus-esp8266`.

![Gateway data flow](images/data-flow.svg)

## What changed

| Area | Before | Now |
| --- | --- | --- |
| Sensor link (RS485 RTU) | `ModbusMaster` with manual DE/RE callbacks | `Wilcoxon883MRTU`; the library drives DE/RE on GPIO 14 |
| Sensor identity | Hand-written function 43/14 frame with a fixed 300 ms wait | Library `readIdentity()`: CRC-checked, paginated, bounded |
| PLC server (Modbus TCP) | `modbus-esp8266`, registers stored in the library | `Wilcoxon883MTCP` reading gateway arrays through `RegisterBlocks` |
| Library version | Not shown | Device Diagnostics: "Modbus Library" |

Unchanged: GPIO assignments, RS485 settings (baud rate and slave ID from Settings, 8N1), the PLC register map, MQTT topics and payloads, web routes and page element IDs, capture sequencing and the watchdog setup.

## Compatibility details

**Sensor reads.** The same input registers are read with the same 2-second response timeout and the same signed 16-bit interpretation and scaling. Errors shown on the dashboard keep their previous codes:

| Code | Meaning |
| --- | --- |
| `0x01`–`0x0B` | Exception returned by the sensor |
| `0xE0` | Invalid response (wrong slave, length or function) |
| `0xE2` | Response timeout (also counted as "timed out") |
| `0xE3` | CRC mismatch |
| `0xE4` | Gateway-side error: UART not ready, write failure or invalid request (new) |

**Identity.** The fields keep their previous object mapping: object 1 is shown as the serial number and used to address MQTT commands, 2 is the sensor firmware, 3 the vendor URL, 4 the part number and 5 the model. The library requires a vendor name and model in the reply; if the sensor omits them, the identity stays at its defaults, as it would after a failed read before.

**PLC server.**

| Behaviour | Before | Now |
| --- | --- | --- |
| Port | Settings (default 502) | Same |
| Clients | Up to 8 | Up to 8; a ninth connection is refused |
| Unit ID | Any | Any |
| Function codes on holding registers | 03, 06, 16, 22 (mask write), 23 (read/write) | 03, 06, 16; 22 and 23 return illegal function |
| Input registers (function 04) | None registered | None registered (function supported by the server) |
| Registers 0–99 | Readable and writable | Same; writes are overwritten by the next sensor update |
| Waveform 1000–14333, spectrum 20000–26144 | Writable copy | Read-only copy (writes return illegal address) |
| Copy timing | Refreshed each second once a capture finished | Same |
| Idle clients | Library default | Closed after 60 s without traffic; a stalled half-sent request after 5 s |

Coil and discrete-input functions were never used by this register map. A PLC that reads holding registers with function 03 needs no change; one that uses function 22 or 23, or polls less often than once a minute, should be checked on the bench.

**Bug fixed while porting.** During a waveform or spectrum download, a chunk whose data could not be stored because the shared mutex was busy was skipped, while the capture could still be reported as successful. The chunk is now retried, and the capture fails if retries run out.

**Still open (unchanged by this migration).** The Modbus TCP settings page documents registers 92/93 as capture triggers, but the firmware does not act on writes to them. Register 90/91 are not reset to 0 when a new capture starts; they show the last completed axis until "Clear cached data".

## Memory and size

Build results for `adafruit_feather_esp32s3` (static allocation reported by PlatformIO):

| Build | RAM (static) | Flash |
| --- | --- | --- |
| Previous libraries (commit 71191c8) | 140,652 bytes (42.9%) | 1,247,213 bytes (86.5%) |
| Wilcoxon883MRTU 0.1.0-beta.4 | 184,476 bytes (56.3%) | 1,241,349 bytes (86.1%) |

The static increase is mainly the PLC copies of the waveform and spectrum (about 39 KB). `modbus-esp8266` stored its registers on the heap at runtime, which the static figures do not include. Compare **Free RAM** on Device Diagnostics on a real gateway before drawing conclusions.

## Bench validation still required

These checks need real hardware and have **not** been performed:

1. **Sensor link:** dashboard metrics match the sensor at 9600 and 115200 baud, identity (model, serial, part number) appears on Device Diagnostics, and an MQTT command addressed to that serial number is accepted.
2. **Captures:** X, Y, Z and XYZ captures complete; MQTT chunk counts are 67 (waveform) and 31 (spectrum); unplugging RS485 mid-download reports a failed capture.
3. **PLC:** using Modbus Poll, QModMaster or a PLC, read 0–60, 90–91, 1000–1124 and 20000–20124; confirm values and scaling against the dashboard; connect several clients; check reconnect after Wi-Fi loss.
4. **Endurance:** run for 24 hours with periodic captures and a polling PLC, watching for watchdog resets and Free RAM trends.

Simulated checks already passing: library native tests (RTU, discovery, TCP), ESP32-S3 compilation of the gateway and all library examples, `node demo/smoke_test.js` and `node demo/browser_test.js`.

## Releasing the beta

Gateway firmware and the library have separate tags:

| Tag | Workflow | Result |
| --- | --- | --- |
| `v4.10.0-beta.1` | [release.yml](../.github/workflows/release.yml) | Gateway `.bin` as a **pre-release**, not Latest; GitHub Packages image tagged with the version only |
| `wilcoxon883mrtu-v0.1.0-beta.4` | [rtu-library-beta.yml](../.github/workflows/rtu-library-beta.yml) | Library source ZIP as a pre-release |

The firmware build embeds the tag version (for example `4.10.0-beta.1`) when built from a `vX.Y.Z` or `vX.Y.Z-beta.N` tag. The gateway updater only installs stable `vX.Y.Z` releases and only loads GitHub's Latest release, so beta pre-releases are never offered to gateways in the field. A gateway running `4.10.0-beta.1` can install stable `v4.10.0` through the updater.

To roll back, install the previous stable `.bin` (v4.9.0) through **Device Diagnostics > Firmware Update (OTA)** or PlatformIO. The updater only installs newer versions, so it cannot downgrade.
