# Changelog

## 0.1.0-beta.4

- Add optional `Wilcoxon883MTCP` Modbus TCP server in the same library: functions 03/04/06/16, up to 8 clients, unit ID filter (default: any), non-blocking sends, pipelined requests, idle and incomplete-frame timeouts.
- Add `RegisterBlocks` to serve existing `uint16_t`/`int16_t` arrays at chosen addresses without copying, with optional lock callbacks and a write notification.
- Add RtuToTcp example using the gateway's holding-register addresses for acceleration RMS, velocity RMS and temperature.
- Add native TCP tests with simulated sockets and an ESP32-S3 compile check; RTU behavior is unchanged.
- Identity also reads optional objects 3 (vendor URL) and 4 (product name), truncated if longer than their buffers.
- Add `WILCOXON883M_LIBRARY_VERSION` in `Wilcoxon883MVersion.h`; packaging checks it against the manifests.
- The gateway firmware in this repository now uses this library instead of ModbusMaster and modbus-esp8266.
- No network, PLC or gateway integration validation claimed.

## 0.1.0-beta.3

- Extend discovery response timeouts for low-baud frame wire time, especially paginated identification at 1200 baud.
- Add low-baud timeout regression checks; keep discovery read-only and globally time-bounded.

## 0.1.0-beta.2

- Read-only discovery for non-default baud rate, parity, stop bits and slave ID.
- Confirm reported settings twice and verify vendor/model with paginated device identity.
- Add all six eight-bit UART formats, scan limits/progress/cancellation and a Busy result.
- Add AutoDiscover example and native discovery/identity tests covering all 16,302 tuples.
- Keep gateway firmware unchanged; no hardware discovery validation claimed.

## 0.1.0-beta.1

- Standalone RTU-only Arduino/ESP32 library with no third-party Modbus dependency.
- Function 03/04 reads, function 06 writes, bounded buffers and response validation.
- Manual-based basic acceleration, velocity and signed temperature conversion.
- Read example, Arduino/PlatformIO manifests, MIT license and simulated transport tests.
- Initial beta: hardware validation remains outstanding.
