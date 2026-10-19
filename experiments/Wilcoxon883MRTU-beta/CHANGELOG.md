# Changelog

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
