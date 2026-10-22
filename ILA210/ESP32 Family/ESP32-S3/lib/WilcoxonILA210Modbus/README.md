# WilcoxonILA210Modbus

Board-local Arduino library for the ILA210 ESP32-S3 gateway. It replaces
ModbusMaster and modbus-esp8266 in this board project. The IRIV IOC project
continues to use its existing communication stack.

## Interfaces

- `WilcoxonILA210RTU.h`: serial RTU client, CRC and response validation,
  FC03/04 reads, FC06 writes, direction callbacks and validated response buffer.
  Reads are limited to 64 registers to preserve the gateway's capture chunks.
- `WilcoxonILA210Registers.h`: imported 14-metric addresses/scales and capture
  constants. Addresses, scaling and unsigned static data types were checked against
  ILA210 manual 97049 Rev.A1. Connected-sensor validation remains required.
- `WilcoxonILA210TCP.h`: bounded TCP sessions and a 50-word holding-register
  cache. Call `addHreg(0, 0, 50)`, `server(port)`, then `task()` frequently.
  Update cached values with `Hreg(address, value)`.

TCP FC03 reads the configured holding cache. Legacy FC06/16 writes update that
cache only: they do not write to the RTU sensor or operate physical outputs.
There is no input-register cache; FC04 returns an illegal-address exception.
The gateway configures eight clients, a five-second frame timeout and a
60-second idle timeout. TCP polling and cache updates belong to one task;
RTU calls also require a single owner or external serialization.

Initialize the UART and existing direction GPIOs in the application before
calling `begin(unit, serial, baud)`. This library does not assign pins.
`readMetric(index, value)` returns zero on success and preserves `value` on
failure. Raw reads expose words through `getResponseBuffer(index)` only after
success; invalid or stale indices return zero. RTU operations use a two-second
response timeout; waiting for bus silence can also consume time.

The generic protocol engines derive from this repository's MIT-licensed
Wilcoxon883MModbus library. ILA metric and capture logic is separate from the
883M sensor mapping. See [LICENSE](LICENSE).

## Verification

Run `python tests/run_native.py` from this library directory with g++ or clang++
available (or the workspace Zig toolchain). Three host suites cover RTU
framing/errors, TCP framing/partial sends, register bounds, and the Arduino
adapter with a simulated UART. Build the enclosing PlatformIO project to check
the actual ESP32 network/UART integration. These checks do not validate physical
RS485 timing, the sensor register map, or connected hardware.
