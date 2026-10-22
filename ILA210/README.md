# Wilcoxon ILA210 gateway

[**➜ Open the web installer**](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/#ila210)

Choose **Wilcoxon ILA210**, then **ESP32-S3** or **IRIV IO Controller**. Releases use separate `ila210-` assets. The public test release is [v4.11.5-beta.2](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/tag/v4.11.5-beta.2): install ESP32-S3 from the browser or download `firmware-ila210-cytron_iriv_io_controller.uf2` for IRIV. Factory ESP32 images flash at `0x0`; application `firmware-*.bin` files are not full-device images. Physical ILA hardware validation is pending. Never flash the 883M images onto an ILA installation. The Explorer name is **Wilcoxon ILA210**; the source folder remains `ILA210/`.

Gateway firmware imported from [Wilcoxon-ILA200-DashboardESP32](https://github.com/dineshpalsingh1989/Wilcoxon-ILA200-DashboardESP32). The workspace entry is called **ILA210**, but the upstream repository says **ILA200** and the firmware identifies itself as **ILA**. Confirm the exact sensor and its register manual before using it on equipment.

The gateway reads sensor metrics, waveforms and spectra over Modbus RTU, serves a local dashboard and Modbus TCP cache, and publishes MQTT messages. It tracks spectrum peaks and learns a statistical amplitude baseline. It does **not** calculate an FFT on the ESP32; the spectrum is read from the sensor.

## Project folders

Board-specific PlatformIO profiles are organized under `ESP32 Family/` and `IRIV IOC/`. Shared firmware stays in `src/`. Open the sensor project root in PlatformIO; its `platformio.ini` loads the board profiles automatically. Existing environment names and build/upload commands are unchanged.

```text
Wilcoxon ILA210/
  ESP32 Family/
    ESP32-S3/              esp32s3
  IRIV IOC/                cytron_iriv_io_controller
  src/                     Shared firmware
  platformio.ini           Project entry point
```

Original ESP32 / Cytron Maker ESP32 is not supported by ILA210.

## Supported boards

| PlatformIO environment | Network | Sensor link | Logging | MQTT |
| --- | --- | --- | --- | --- |
| `esp32s3` | Wi-Fi | External RS485 transceiver | RAM / external SD | Plain or verified TLS |
| `cytron_iriv_io_controller` | W5500 Ethernet | Built-in isolated RS485 | RAM | Plain, port 1883 |

The [Cytron IRIV IO Controller](https://my.cytron.io/p-iriv-io-controller) uses an RP2350, rather than an ESP32. Its alert/alarm outputs use DO0/DO1, with the onboard monochrome status LED. Both networking tasks stay on core 0 as required by the Arduino-Pico network stack. See [hardware wiring diagrams](docs/wiring.md).

## IOC I/O and alarms

On IRIV, open **Settings > IOC I/O & Alarms** for separate live-status, digital input/push-button, analog/4?20 mA and alarm-output pages. Configure capture/acknowledge buttons, low/high analog alarms with hysteresis, and a beacon/sounder on DO2/DO3. DO0/DO1 retain the vibration alarm assignments. New functions default to disabled. Physical analog mode switches must match the software setting. See the [I/O setup guide](docs/io-alarms.md) and [wiring diagrams](docs/wiring.md).

**View without hardware:** run `node ILA210/demo/mock_server.js` from the parent workspace, then open **http://localhost:8091/ioc**. The [local demo](demo/README.md) renders the actual IOC pages with clearly labeled simulated signals, push-buttons and alarms. It does not connect to or change a device.

## Build

The firmware source is now [src/main.cpp](src/main.cpp), replacing the imported `ILA_Firmware.in`. [platformio.ini](platformio.ini) pins the dependencies and uses an ESP32-S3 DevKitC-1 N8 build target. This is a compile target, not confirmation of your physical board. The ESP32-S3 3 MB application partition has no OTA slot. The IRIV target reserves 1 MB for firmware and 1 MB for its LittleFS settings store. No OTA installer is implemented.

```powershell
pio run -d ILA210 -e esp32s3
pio run -d ILA210 -e cytron_iriv_io_controller
```

Run that command from the parent workspace, or use `pio run` inside this folder. In this workspace the Python entry point is `../.venv/Scripts/python.exe -m platformio`. The workspace-local cache is selected with `PLATFORMIO_CORE_DIR=../.pio-core` (use an absolute path if invoking from another directory).

**ESP32-S3:** First boot without saved Wi-Fi settings starts `ILA_Sensor_AP`, password `12345678`. Open `http://192.168.4.1/` and configure Wi-Fi. Settings and controls currently have no authentication; operate on a trusted network.

**IRIV:** Plug in Ethernet. DHCP is attempted for 10 seconds; without an answer, the gateway uses `192.168.4.1` (set the connected laptop to `192.168.4.2/24`). Holding USER for five seconds also switches to this fallback address. Wi-Fi setup/scan operations are unavailable on this board. Logging stays in RAM; GPIO5 is never used as an SD chip select.

## Hardware configuration

Existing explicit ESP32-S3 GPIO assignments are preserved:

| Function | GPIO |
| --- | --- |
| RS485 RX / TX / DE-RE | 16 / 18 / 10 |
| Alert / alarm relay | 1 / 2 |
| NeoPixel | 46 |
| SD chip select | 5 |

The imported README claimed SD SCK 18, MISO 19 and MOSI 23, while the firmware uses the board's default `SPI` pins. GPIO 18 is already RS485 TX. **Do not wire from that old table.** For the selected ESP32-S3 build variant, the framework defaults are SCK 12, MISO 13 and MOSI 11. Verify the exact board, supply levels and SD wiring before connecting it. No sensor connector pin numbers or cable colors have been assumed. See [review and bench checks](docs/review.md).

## MQTT

Address commands with the hardware-derived `gateway_id` shown in Device Info or [sensor communication settings](docs/sensor-communication.md). Example: `{"gateway_id":"<your gateway ID>","command":"get_status"}` on the configured commands topic. Legacy `serial_number` command targeting is rejected; update integrations when upgrading. The sensor serial is unverified and telemetry reports an empty value rather than the imported placeholder. Base topics and register mappings are unchanged.


The default base topic is `ILA`; the command and status topics follow the configured base as `<topic>/command` and `<topic>/status`. Each gateway uses its MAC address in the broker client ID. JSON timestamps are UTC.

Port **1883** explicitly selects plain MQTT on both boards. IRIV supports only this transport. On ESP32-S3, other ports use TLS with certificate verification. For TLS, create an **untracked** `src/local_mqtt_ca.h` containing `#define ILA_MQTT_ROOT_CA` with your broker's PEM root CA as a C++ string. Never commit that file or credentials. TLS MQTT stays disabled without a configured CA; plain MQTT is not selected automatically on failure. The clock must synchronize for certificate date checks.

Waveform publication sends one bounded chunk per background pass. Reconstruct chunks using `capture_id`, `chunk_index` and `total_chunks`; check completeness because QoS 0 delivery is not guaranteed. Messages are not retained. A full waveform has 25,600 samples; the preview has 128 samples. Sensor register addresses, scaling and sample counts remain imported and require manual/bench confirmation.

## Logging and checks

The [IOC review procedure](docs/ioc-review-agent.md) describes the audit scope and required evidence. Local Codex agent definitions are kept outside the public repository. See the [current IOC deployment-readiness assessment](docs/ioc-readiness.md) for confirmed issues, software-check evidence and the pending physical bench checklist.

RAM histories are bounded and lost at reboot. Each SD CSV rotates at approximately 1 MiB and retains one `.previous` file. CSV download endpoints return the current file. Long downloads may temporarily skip log writes when storage is busy. IRIV has no SD logging; LittleFS stores settings only.

The dashboard's CSS, JavaScript and canvas charts are local to the firmware. No CDN is needed. To run simulated browser checks from the parent workspace:

```powershell
node ILA210/tests/browser_test.js
node ILA210/tests/ioc_browser_test.js
```

The test uses Playwright in the parent workspace's `.tools/node_modules` (or this project's `.tools/node_modules`) and installed Chrome. For a standalone checkout: `npm.cmd install --prefix .tools --no-package-lock playwright`. Set `CHROME_PATH` to override the Chrome executable.

Browser tests simulate the HTTP API. They do not validate the RTU sensor, electrical connections, real MQTT/TLS, SD card, relays or FreeRTOS timing. Read [docs/review.md](docs/review.md) for the fixes and outstanding validation.
