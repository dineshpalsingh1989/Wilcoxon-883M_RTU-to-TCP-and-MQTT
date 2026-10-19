<div align="center">

# Wilcoxon 883M Industrial IoT Gateway

**Bring RS485 vibration data to your browser, PLC and MQTT broker.**

[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Arduino-00979D)](platformio.ini)
[![Modbus](https://img.shields.io/badge/Modbus-RTU%20%2B%20TCP-2563eb)](#modbus-tcp)
[![MQTT](https://img.shields.io/badge/MQTT-TLS%20supported-7c3aed)](#mqtt)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

[Quick start](#quick-start) · [Dashboard preview](#dashboard-preview) · [Wiring](#hardware-and-wiring) · [MQTT](#mqtt) · [Firmware updates](#firmware-updates)

</div>

![System overview: Wilcoxon 883M sends Modbus RTU data through an ESP32-S3 gateway to a web dashboard, PLC or SCADA, and MQTT broker.](docs/images/system-overview.svg)

Firmware for an ESP32-S3 gateway connected to a **Wilcoxon 883M vibration sensor** through an RS485-to-TTL interface. Monitor machine vibration and temperature, capture waveforms and FFT spectra, and share readings over Wi-Fi.

## What you can do

| Feature | What it provides |
| --- | --- |
| Browser dashboard | Live X/Y/Z metrics, velocity and temperature trends, and waveform/spectrum charts |
| Sensor health | Connection status, failed reads, timeouts and age of the last successful reading |
| Configurable alarms | Velocity RMS and temperature thresholds with a configurable delay |
| Capture tools | Single-axis or XYZ captures, capture history and CSV export |
| MQTT integration | Scalar metrics, chunked waveform/spectrum data, health status and remote commands |
| Modbus TCP bridge | Cached measurements and capture buffers for PLC and SCADA clients |
| Gateway management | Wi-Fi, RS485, MQTT and time settings, plus GitHub firmware installation |

## Dashboard preview

![ESP32 gateway dashboard showing simulated sensor health, vibration metrics, trends and capture controls.](docs/images/dashboard.png)

*Actual dashboard rendered by the hardware-free demo with simulated readings. Values shown are illustrative.*

## Quick start

### Try it without hardware

Install Node.js, then run:

```sh
git clone https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT.git
cd Wilcoxon-883M_RTU-to-TCP-and-MQTT
node demo/mock_server.js
```

Open **http://localhost:8080**. Use the demo scenario selector to explore normal operation, high vibration and an offline sensor. Settings and firmware update actions are simulated. See the [demo guide](demo/README.md) for details.

### Build and flash the gateway

1. Install PlatformIO and open this repository in VS Code.
2. Check the board and pin assignments below against your hardware.
3. Connect the ESP32-S3 over USB and run:

```sh
pio run
pio run --target upload
pio device monitor
```

The configured environment is `adafruit_feather_esp32s3`, using the Arduino framework. PlatformIO installs the libraries declared in [platformio.ini](platformio.ini). The build helper also requires Python's `certifi` package in the PlatformIO Python environment.

### Connect to Wi-Fi

1. Connect to the gateway access point **`ILA_Sensor_AP`**, password **`12345678`**.
2. Open **http://192.168.4.1**.
3. Open **Settings > Wi-Fi** and save your network credentials.
4. Open the gateway's assigned local IP address to use the dashboard.

## Hardware and wiring

![ESP32-S3 to RS485 transceiver and Wilcoxon 883M functional wiring](docs/images/wiring.svg)

Follow the [step-by-step wiring and troubleshooting guide](docs/wiring.md) before powering the hardware.

| Component | Purpose |
| --- | --- |
| ESP32-S3 board | Gateway; project hardware targets the Cytron Maker Feather AIoT S3 |
| Wilcoxon 883M | RS485 Modbus RTU vibration and temperature sensor |
| RS485-to-TTL UART module | Interface with ESP32-compatible 3.3 V logic levels |
| Suitable DC supply | Sensor power; check the linked sensor manual for supply and wiring requirements |

**Current firmware assignments** from [src/config/config.h](src/config/config.h):

| ESP32 GPIO | Connection / function |
| --- | --- |
| 18 | RS485 module RO → UART RX |
| 17 | UART TX → RS485 module DI |
| 14 | RS485 module DE and RE direction control |
| 5 | Buzzer |
| 0 | User button |
| 48 | RGB status LED |

Connect the sensor's RS485 pair to the module's A/B terminals according to the hardware documentation. Confirm grounding and termination for your installation. Board labels can differ from GPIO numbers; verify the selected board before applying power.

[Wilcoxon 883M user manual (PDF)](883M-User-Manual-Rev.1.0_20241206-EN.pdf)

## MQTT

The default base topic is **`883M`**, configurable in Settings. TLS is supported.

| Topic | Direction | Content |
| --- | --- | --- |
| `883M` | Gateway → broker | Metrics, waveform chunks and spectrum chunks |
| `883M/status` | Gateway → broker | Health status, normally every five minutes |
| `883M/command` | Broker → gateway | Commands addressed to the sensor serial number |

Every command must include a **`serial_number` matching the connected sensor**.

```json
{
  "serial_number": "ENTER_DEVICE_SERIAL_HERE",
  "command": "trigger_capture",
  "axis": "X"
}
```

| Command | Additional fields | Action |
| --- | --- | --- |
| `trigger_capture` | `"axis": "X"`, `"Y"` or `"Z"` | Capture waveform and spectrum for one axis |
| `trigger_all` | None | Start an XYZ capture sequence |
| `get_metrics` | None | Publish scalar metrics immediately |
| `set_interval` | `"interval_ms": 600000` | Set the automatic capture interval; example is 10 minutes |
| `reboot` | None | Restart the gateway |

Set `interval_ms` to `2147483647` to use the firmware's disabled auto-capture interval. Captures are refused while another capture or firmware update is active.

Waveform and spectrum messages use `type`, `axis`, `capture_id`, `chunk_index`, `total_chunks` and `data`. Reassemble chunks in index order for each capture and axis. Chunks carry up to **200 points**: 67 chunks for a 13,334-point waveform and 31 for a 6,145-point spectrum.

For plots, waveform values represent acceleration in g; time depends on the sample interval. Spectrum values represent amplitude in g; frequency is the bin index multiplied by the firmware's frequency resolution (`13333.333 / 16384` Hz). Timestamps use configured local time and do not carry a UTC `Z` suffix.

## Modbus TCP

The gateway serves cached data on the configured Modbus TCP port, normally **502**. Values are **holding registers**, using zero-based addresses. Divide raw values by the scale below to obtain engineering units.

| Address | Measurement | Scale | Unit |
| --- | --- | --- | --- |
| 0 | X acceleration RMS | 100 | g |
| 1 | X acceleration peak | 100 | g |
| 2 | X acceleration RMS, 10–5k Hz | 100 | g |
| 3 | X acceleration peak, 10–5k Hz | 100 | g |
| 4 | X velocity RMS | 10 | mm/s |
| 5 | X velocity peak | 10 | mm/s |
| 6 | X displacement RMS* | 1 | µm |
| 7 | X displacement peak | 1 | µm |
| 8 | X true peak | 100 | g |
| 9 | X crest factor | 100 | — |
| 20–37 | Y metrics, same layout as X addresses 0–17 | Varies | Varies |
| 40–57 | Z metrics, same layout as X addresses 0–17 | Varies | Varies |
| 60 | Temperature | 10 | °C |
| 1000–14333 | Latest-axis waveform, signed Int16 | 1000 | g |
| 20000–26144 | Latest-axis spectrum, UInt16 | 1000 | g |

*Displacement RMS is not currently read from the sensor. See [the register cache implementation](src/modbus/modbus_tcp.cpp) and [register constants](src/config/config.h) for the complete mapping and capture controls.*

## Firmware updates

Open **Settings > GitHub Firmware Updates** to check for a newer stable release and install it on the **ESP32 gateway**.

The updater requires a stable `vMAJOR.MINOR.PATCH` release, the asset `firmware-adafruit_feather_esp32s3.bin`, and GitHub's SHA-256 asset digest. It validates HTTPS certificates, image compatibility, partition size and the downloaded checksum before activating the image and rebooting. Wi-Fi and a correct gateway clock are required.

See the [update guide](demo/README.md#github-firmware-installation) for release publishing and validation details. Updates apply to the gateway; they do not update the Wilcoxon sensor firmware.

## Operational notes

- Trend history (120 successful snapshots) and capture history (20 results) are stored in RAM and reset on reboot. Alarm settings persist on the real gateway.
- Dashboard alarms are disabled until configured. They do not control machinery or the buzzer; choose limits appropriate to the machine.
- Web and OTA endpoints currently have no authentication. Deploy the gateway on a trusted network.
- Firmware integrity checks do not provide signature verification or application-health rollback. Actual sensor operation and flash updates still require validation on the target hardware.

## Contributing with coding assistants

[AGENTS.md](AGENTS.md) documents the project structure, coding rules and validation commands for coding assistants.

## Project layout

| Location | Contents |
| --- | --- |
| `src/config/`, `src/core/` | Pin/register definitions, shared state and monitoring |
| `src/modbus/` | Sensor RTU communication and TCP register cache |
| `src/capture/`, `src/mqtt/` | Capture state machine, logs, publishers and commands |
| `src/web/` | Dashboard, settings pages and HTTP routes |
| `src/ota/` | Firmware upload and GitHub release installation |
| `src/hardware/`, `src/tasks/`, `src/utils/` | Hardware helpers, FreeRTOS tasks and utilities |
| `demo/` | Hardware-free preview and browser checks |
| `scripts/` | Build configuration helpers |

## License

Released under the [MIT License](LICENSE).
