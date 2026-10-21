# Wiring the gateway

![Functional wiring diagram for the ESP32-S3, RS485 interface, sensor and separate sensor supply](images/wiring.svg)

This is a **functional connection diagram**, not a sensor connector pinout. Identify the 883M power and RS485 terminals from the [bundled manufacturer manual](../883M-User-Manual-Rev.1.0_20241206-EN.pdf) and the labels on your exact sensor/cable. Connector pin numbers and wire colors are intentionally not assumed.

## Connections

With power disconnected, wire a **3.3 V logic-compatible, non-isolated RS485 transceiver** as follows:

| From | To |
| --- | --- |
| ESP32 GPIO17 (TX) | Transceiver DI |
| ESP32 GPIO18 (RX) | Transceiver RO |
| ESP32 GPIO14 | Both DE and /RE (active-low receiver enable) |
| ESP32 GND | Transceiver GND and sensor signal reference, as specified by the sensor manual |
| Compatible logic supply | Transceiver VCC, according to its module specification |
| Transceiver A | Sensor RS485 A terminal |
| Transceiver B | Sensor RS485 B terminal |
| Sensor supply positive | Sensor power-positive terminal |
| Sensor supply return | Sensor power-return terminal |

Power the ESP32 through its supported USB/power input. Use the sensor's specified DC supply, and never connect that supply directly to an ESP32 GPIO or its 3.3 V rail. A 5 V-only receiver output must not drive an ESP32 GPIO directly. For an isolated RS485 module, follow its own bus-side power/reference instructions rather than bridging the isolation barrier.

## Before the first read

1. Verify the sensor supply voltage and connector assignments against the manual.
2. Verify GPIO numbers against [the firmware definitions](../src/config/config.h), rather than relying on board header position.
3. Check the transceiver's supply and output logic levels. DE and /RE must support the shared direction-control connection.
4. Use a twisted pair for A/B. Fit termination at the physical bus ends according to the equipment instructions and account for termination/bias resistors already fitted to modules.
5. In Settings, match the sensor slave ID and serial settings. Start with one sensor on a short bench cable.
6. Confirm that sensor health reports successful readings before connecting a PLC or enabling alarms.

## If the sensor does not respond

- Check sensor power and the signal reference first.
- Confirm slave ID and serial settings match the sensor.
- Verify TX goes to DI, RX goes to RO, and GPIO14 drives both enable inputs.
- Check A/B naming against both manufacturers' documentation; naming conventions can differ.
- Check termination, biasing and cable routing if communication is intermittent.

The RS485 wiring above is the same for every [supported board](../README.md#supported-boards). On the ESP32-S3 build the RGB LED, buzzer and user-button assignments are GPIO48, GPIO5 and GPIO0; verify whether these are already onboard before adding external components. On the Cytron Maker ESP32 build they are the board's onboard GPIO2 LED, GPIO26 piezo buzzer and GPIO4 user button, so nothing extra is wired. The Maker ESP32's onboard GPIO17 and GPIO18 LEDs flicker with RS485 traffic.
