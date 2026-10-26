# Wilcoxon 883M / IRIV IOC

This is a complete, independent PlatformIO project. Edit this board's files here; changes do not automatically modify other boards.

- [src/main.cpp](src/main.cpp): firmware entry point
- [src/](src/): HTML/web pages, sensor communication and hardware code
- [lib/](lib/): local libraries; downloaded dependencies are declared in `platformio.ini`
- [scripts/](scripts/): build, version and installer helpers
- `platformio.ini`: standalone project configuration
- `profile.ini`: board settings also used by the sensor-root build selector

From the Wilcoxon workspace root:

```powershell
pio run -d "IRIV IOC"
```

Open this folder directly in PlatformIO to build it independently. Review the sensor wiring guide before any hardware upload.

HTML and HTTP handlers are in [src/web/](src/web/). Sensor communication is in [src/modbus/](src/modbus/) and [lib/Wilcoxon883MRTU/](lib/Wilcoxon883MRTU/).

Experimental reusable libraries:

- [WilcoxonMqtt](lib/WilcoxonMqtt/README.md): the gateway's MQTT client (plain MQTT, and the protocol layer under TLS). It replaces PubSubClient.
- [W5500SecureMqtt](lib/W5500SecureMqtt/README.md): used by this gateway for MQTT over certificate-validated TLS with Arduino-Pico BearSSL and WilcoxonMqtt.
- [IRIVIOCHardware](lib/IRIVIOCHardware/README.md): configurable Ethernet setup and bounded LittleFS settings using ArduinoJson.

All are alpha libraries requiring hardware validation. IRIVIOCHardware remains a standalone library. The gateway and [standalone MQTT example](lib/W5500SecureMqtt/examples/IRIVSecurePublish/main.cpp) run networking in Arduino setup/loop.

The gateway's **Settings → Our Libraries** page (`/libraries`) shows the version of each library built into the firmware, their live state and last errors, and has one-click tests: a sensor register read and identity read, MQTT test messages and a settings-storage write and read-back. **Copy diagnostics** copies the whole page as text for a problem report.

## IOC I/O (beta, not yet validated on hardware)

Pins are from the Cytron IRIV IO Controller datasheet (via the board variant). Settings → **IOC I/O** (`/io`) shows the live state, acknowledges the alarm and sets up the analog inputs.

| Terminal | RP2350A | Gateway use |
| --- | --- | --- |
| DO0–DO3 | GP12–GP15 | Each relay follows one source chosen on the IOC I/O page (defaults: DO0 dashboard alarm, latched; DO1 sensor fault; DO2/DO3 off) |
| DI0 | GP0 | Rising edge starts an XYZ capture (ignored while busy or while the sensor is paused) |
| DI1 | GP1 | Rising edge acknowledges the alarm |
| DI2–DI10 | GP2–GP10 | Reported only |
| AN0, AN1 | GP26, GP27 | 4–20 mA or 0–10 V (match the AN-IN MODE DIP switch), scaled to a named value with a unit |
| USER button | GP28 | Short press acknowledges; holding 5 s still selects the fixed address 192.168.4.1 |

Relay sources: **Dashboard alarm** (the dashboard's vibration/temperature limits), **Sensor fault** (no answer, or polling stopped after 3 failed reads), **883M value** (one of 14 values the gateway reads, on X, Y, Z or the highest of the three, at or above / at or below a threshold) or **AN0/AN1** (the scaled analog value against a threshold). Each rule has a hysteresis (release margin), a delay and an optional latch. A source, or an 883M value on overlapping axes, can drive only one output; the page greys out choices already in use and the gateway rejects duplicates. 883M rules are off while the sensor has no fresh reading; analog rules are off while the input is disabled or faulted.

Acknowledging (web page, USER button or DI1) silences the buzzer, lets latched relays drop once their condition has cleared and retries a paused sensor. A 4–20 mA input below 3.6 mA or above 21 mA, or any over-range reading, is reported as a fault. Analog readings use the nominal ADC reference and are not calibrated. Outputs are solid-state relays rated 50 V / 500 mA (see the datasheet); add a flyback diode across relay coils.

MQTT: `<base topic>/io`, type `io_status`, sent on every change and at least once a minute, not retained.

Modbus TCP holding registers 70–79: 70 DI bit mask (bit n = DIn), 71 DO bit mask, 72 status bits (1 dashboard alarm active, 2 a relay is latched, 4 acknowledged, 8 sensor fault), 73 rule-condition bit mask (bit n = DOn's rule is true), 74/77 AN0/AN1 scaled value ×10 (Int16, clamped), 75/78 AN0/AN1 terminal mA or V ×100, 76/79 AN0/AN1 state (0 off, 1 OK, 2 fault).

If the sensor does not answer three metrics reads in a row, the gateway stops polling it and the dashboard shows **Reconnect sensor**.

**MQTT over TLS (port 8883) is not supported on this board.** On bench tests the broker closed TLS sessions about 2 seconds after connecting, so the MQTT page disables TLS and a TLS setting saved earlier is ignored; use plain MQTT on port 1883 on a trusted network or through a local broker. The guide below is kept for that unfinished work.

See the [secure MQTT beta installation guide](docs/secure-mqtt-beta.md) for the full gateway firmware, CA certificate setup and remaining hardware tests.
