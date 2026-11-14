# Validation record

Date: 2026-10-24. Status: experimental alpha; no hardware or live broker verification.

The standalone IRIVSecurePublish example compiles and links both W5500SecureMqtt and IRIVIOCHardware with FreeRTOS enabled. Target: cytron_iriv_io_controller, RP2350, Arduino-Pico package 1.60100.0+sha.fd65f6d4, Raspberry Pi PlatformIO commit 5d4561a05e3b212660ac6fdd3fbfb328d1988aa1. Dependencies: PubSubClient 2.8.0 and ArduinoJson 6.21.6.

Run from the workspace root:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path (Get-Location) '.pio-core'
.venv/Scripts/python.exe -m platformio run -d 'IRIV IOC/lib/W5500SecureMqtt/tests/iriv_compile'
```

The local check reused downloaded dependency packages via `PLATFORMIO_LIBDEPS_DIR` pointing at the IOC project's `.pio/libdeps` directory. That cache override is optional and is not part of the library interface.

Compilation validates API compatibility and linkage. It does not exercise TLS, settings persistence, watchdog behavior or reconnect timing. Static RAM/flash reports exclude dynamic TLS allocations. Execute the hardware scenarios in [README.md](README.md) and verify settings write/read, read-only behavior, overflow rejection, mount failure and interrupted writes before release or gateway integration.

On 2026-10-25 the full IRIV gateway was integrated with W5500SecureMqtt; see the gateway secure MQTT beta guide for that separate validation record. Neither library has been published to GitHub or a package registry.
