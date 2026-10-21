<div align="center">

# Wilcoxon 883M Industrial IoT Gateway

**Bring RS485 vibration data to your browser, PLC and MQTT broker.**

[![ESP32-S3, ESP32 and RP2350](https://img.shields.io/badge/ESP32--S3%20%7C%20ESP32%20%7C%20RP2350-Arduino-00979D)](#supported-boards)
[![Modbus](https://img.shields.io/badge/Modbus-RTU%20%2B%20TCP-2563eb)](#modbus-tcp)
[![MQTT](https://img.shields.io/badge/MQTT-TLS%20supported-7c3aed)](#mqtt)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

[![Install on your ESP32 from the browser](https://img.shields.io/badge/Install%20on%20your%20ESP32-from%20the%20browser-0b6bcb?style=for-the-badge)](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/)

[Install](#install-on-your-esp32-in-4-steps) · [Quick start](#quick-start) · [Dashboard preview](#dashboard-preview) · [Wiring](#hardware-and-wiring) · [MQTT](#mqtt) · [Firmware updates](#firmware-updates)

</div>

![System overview: Wilcoxon 883M sends Modbus RTU data through an ESP32-S3 gateway to a web dashboard, PLC or SCADA, and MQTT broker.](docs/images/system-overview.svg)

Firmware for an ESP32-S3 or ESP32 gateway connected to a **Wilcoxon 883M vibration sensor** through an RS485-to-TTL interface. Monitor machine vibration and temperature, capture waveforms and FFT spectra, and share readings over Wi-Fi.

## Install on your ESP32 in 4 steps

**You can put this gateway on your own ESP32 board straight from your browser.** No PlatformIO, Arduino IDE or library downloads: the firmware already contains everything, including the Wilcoxon883MRTU Modbus RTU/TCP library.

| You need | |
| --- | --- |
| Board | **Cytron Maker ESP32**, **Adafruit Feather ESP32-S3** or **Cytron IRIV IO Controller**, see [Supported boards](#supported-boards) |
| Cable | USB-C **data** cable |
| Computer | Chrome or Edge on Windows, macOS, Linux or ChromeOS |
| Sensor wiring (afterwards) | RS485-to-TTL module and Wilcoxon 883M, see [Hardware and wiring](#hardware-and-wiring) |

### Supported boards

This gateway supports **three boards**. On the [web installer](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/), first choose **ESP32 board** or **IRIV IO Controller**:

1. **Adafruit Feather ESP32-S3** (ESP32-S3 chip): ESP32 board, Wi-Fi
2. **Cytron Maker ESP32** (original ESP32 chip): ESP32 board, Wi-Fi
3. **Cytron IRIV IO Controller** (RP2350A chip): industrial controller with isolated RS485 and **wired Ethernet**; installs by copying a `.uf2` file, see [IRIV IO Controller](#iriv-io-controller)

**For ESP32 boards the installer finds the chip automatically.** When you click **Install** and pick the port, it reads the chip on the connected board and installs the matching build. There is nothing to choose.

**Any chip not in this list is refused.** If the board has an ESP32-S2, ESP32-C3, ESP32-C6, ESP32-H2 or any other chip, the installer stops with *"This device is not supported"* and writes nothing to the board.

| Board | Chip | Build installed | Status LED | Buzzer | User button |
| --- | --- | --- | --- | --- | --- |
| Adafruit Feather ESP32-S3 | ESP32-S3 | `adafruit_feather_esp32s3` | RGB LED, GPIO48 | GPIO5 | GPIO0 (BOOT) |
| [Cytron Maker ESP32](https://my.cytron.io/p-maker-esp32-bundle) | ESP32 (ESP32-WROOM-32E, 8 MB flash) | `cytron_maker_esp32` | Onboard LED on GPIO2 | Onboard piezo, GPIO26 | Onboard button, GPIO4 |
| Cytron IRIV IO Controller | RP2350A (2 MB flash) | `cytron_iriv_io_controller` (`.uf2`) | USR LED, GP29 | Internal buzzer, GP11 | USER button, GP28 |

Both ESP32 builds use the **same RS485 wiring**: GPIO18 (RX), GPIO17 (TX) and GPIO14 (DE/RE). The IRIV IO Controller has RS485 built in. See [Hardware and wiring](#hardware-and-wiring).

> **Cytron Maker ESP32 is supported from v4.10.0.** The original ESP32 has less RAM than the ESP32-S3, so the firmware sends large pages in parts and sets its capture buffers aside at startup. If the GitHub updater ever reports a memory or TLS error, update by uploading `firmware-cytron_maker_esp32.bin` under **Settings > Device Diagnostics > Firmware Update (OTA)** instead.
>
> On the Maker ESP32 the GPIO2 LED is **on** when the gateway is online, **blinks** during captures and metric recording, and is **off** when offline or after a sensor error. GPIO17 and GPIO18 also have onboard activity LEDs, so they flicker with RS485 traffic. The buzzer's mute switch silences the Wi-Fi alarm beeps.

### IRIV IO Controller

> **New and not yet tested on hardware.** The IRIV build compiles and runs the same gateway (dashboard, Modbus TCP, MQTT, captures), but it has not yet been run on a real IRIV with an 883M sensor.

The IRIV IO Controller is a DIN-rail controller with an **isolated RS485** port (automatic direction control, switchable 120 Ω termination) and **wired Ethernet**. It has no Wi-Fi, so the Wi-Fi pages become a **Network** page.

1. **Install:** on the web installer choose **IRIV IO Controller** and click **Download .uf2**. Connect the IRIV's USB-C (PROG) port, hold the **BOOT** pinhole button, press **RESET**, release **BOOT**, and copy the file to the **RP2350** drive that appears.
2. **Connect:** plug in Ethernet. The gateway gets its address by DHCP and answers at **http://ila-gateway-883m.local**. If no DHCP server answers within 10 seconds, it uses **192.168.4.1** (set your laptop to 192.168.4.2, mask 255.255.255.0). Holding **USER** for 5 seconds also switches to 192.168.4.1.
3. **Wire the sensor** to the IRIV's RS485 **A, B, G** terminals, see [the wiring guide](docs/wiring.md#cytron-iriv-io-controller).
4. **Update:** upload `firmware-cytron_iriv_io_controller.bin` under **Settings > Device Diagnostics > Firmware Update (OTA)**, or copy a new `.uf2`. Both keep settings. **Settings > GitHub Firmware Updates** cannot install directly on this board yet.

> **Other boards with the same chip are not supported.** The installer checks the chip, not the board name. So another ESP32-S3 board will be offered the Feather build, and another original-ESP32 board will be offered the Maker ESP32 build, which needs 8 MB flash. They may not start, or may drive the wrong pins. Use one of the two boards above, or check [`src/config/config.h`](src/config/config.h) against your hardware and build from source.

<p align="center"><a href="https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/"><strong>➜ Open the web installer</strong></a></p>

![Four install steps: open the web installer, plug in the Feather over USB-C, click Install and pick the port, then press RESET and join the ILA_Sensor_AP Wi-Fi network.](docs/images/install-steps.svg)

1. **Open the [web installer](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/)** in Chrome or Edge on a computer.
2. **Plug the board in** with a USB-C data cable.
3. **Click Install** and pick the board's port. It takes about a minute. If no port appears, hold **BOOT**, tap **RESET** (labelled **EN** on the Maker ESP32), release **BOOT** and try again.
4. **Press RESET** (or **EN**), join the Wi-Fi network **`ILA_Sensor_AP`** (password `12345678`), open **http://192.168.4.1** and enter your own Wi-Fi under **Settings > Wi-Fi**.

<table>
<tr>
<td width="60%"><img src="docs/images/web-installer.png" alt="Web installer page with an Install button next to the available firmware version."></td>
<td width="40%"><img src="docs/images/wifi-setup.png" alt="Gateway Wi-Fi Settings page with a network selected, password entered and a Save and Connect button."></td>
</tr>
<tr>
<td><em>Step 3: the web installer. The version shown is an example; the page lists the current releases.</em></td>
<td><em>Step 4: Wi-Fi Settings on the gateway (simulated demo).</em></td>
</tr>
</table>

After saving, the gateway restarts and joins your network. Open its new IP address to see the dashboard, then [wire the sensor](#hardware-and-wiring).

### If the install does not work

Re-installing is always safe: the ESP32's built-in loader cannot be erased, so you can simply try again.

| Problem | Fix |
| --- | --- |
| No port appears | Use a USB-C **data** cable, try another USB socket (no hub), then hold **BOOT**, tap **RESET**, release **BOOT** and try again |
| No port appears (Maker ESP32, Windows or macOS) | Install the WCH USB-serial driver (CH9102/CH343) for the board's USB chip, then reconnect |
| "This device is not supported" | The board's chip is not ESP32-S3 or ESP32, or the selected release is older than Maker ESP32 support |
| "Failed to connect" or timeout | Close Arduino IDE, PlatformIO or any serial monitor using the port, then use BOOT + RESET as above |
| Stops halfway | Change the cable, avoid hubs and install again |
| Finished but nothing happens | Press **RESET**. Still no `ILA_Sensor_AP`? Install again with *Erase device* ticked |
| Browser not supported | Use Chrome or Edge on a computer, or one of the other ways below |

**Other ways to install the same firmware:**

| Way | Use when |
| --- | --- |
| [Espressif browser flasher](docs/installing-firmware.md#2-espressif-browser-flasher): write `factory-<board>.bin` at `0x0` | The installer page fails but Chrome or Edge works |
| [esptool command line](docs/installing-firmware.md#3-esptool-command-line): `python -m esptool write_flash 0x0 factory-<board>.bin` | Any computer with Python, no browser needed |
| [Espressif Flash Download Tool](docs/installing-firmware.md#4-espressif-flash-download-tool-windows) | Windows users who prefer a desktop app |
| [Build from source](#build-and-flash-the-gateway) with PlatformIO | Developers |

`<board>` is `adafruit_feather_esp32s3` or `cytron_maker_esp32` ([Supported boards](#supported-boards)). The factory file is on each [release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases) and clears saved settings. The [installation guide](docs/installing-firmware.md) has step-by-step instructions for each way.

## Built on our own Modbus library

> From **v4.10.0** the gateway uses this project's own **Wilcoxon883MRTU 0.1.0-beta.4** library for both sides of the gateway. The third-party `ModbusMaster` and `modbus-esp8266` libraries are no longer used. Register addresses, MQTT messages and web pages are unchanged. The library itself is still versioned as a beta while its API may change.

![Gateway data flow: the 883M is read by the Wilcoxon883MRTU client into gateway memory, and the Wilcoxon883MTCP server answers PLCs from that memory alongside the web dashboard and MQTT.](docs/images/data-flow.svg)

| Part | Job | Replaces |
| --- | --- | --- |
| `Wilcoxon883MRTU` | Reads the sensor over RS485: metrics, captures, device identity | `ModbusMaster` |
| `Wilcoxon883MTCP` | Serves the gateway's registers to PLCs/SCADA over Wi-Fi | `modbus-esp8266` |

The library lives in [experiments/Wilcoxon883MRTU-beta](experiments/Wilcoxon883MRTU-beta/README.md) and is compiled into the firmware from this repository, so a gateway firmware update always brings the matching library version. **Settings > Device Diagnostics** shows which version is installed. The library can also be used on its own in Arduino or PlatformIO sketches; see its [installation guide](experiments/Wilcoxon883MRTU-beta/README.md). The [library migration guide](docs/modbus-library.md) covers compatibility, memory use, the beta release process and the bench tests still needed.

## What you can do

| Feature | What it provides |
| --- | --- |
| Browser dashboard | Live X/Y/Z metrics, velocity and temperature trends, and waveform/spectrum charts |
| Sensor health | Connection status, failed reads, timeouts and age of the last successful reading |
| Configurable alarms | Velocity RMS and temperature thresholds with a configurable delay |
| Capture tools | Single-axis or XYZ captures, capture history and CSV export |
| MQTT integration | Scalar metrics, chunked waveform/spectrum data, health status and remote commands |
| Modbus TCP server | Cached measurements and capture buffers for up to 8 PLC and SCADA clients |
| Gateway management | Wi-Fi, RS485, MQTT and time settings, plus GitHub firmware installation |

## Dashboard preview

![ESP32 gateway dashboard showing simulated sensor health, vibration metrics, trends and capture controls.](docs/images/dashboard.png)

*Actual dashboard rendered by the hardware-free demo with simulated readings. Values shown are illustrative.*

<table>
<tr>
<td width="30%"><img src="docs/images/dashboard-mobile.png" alt="Dashboard on a phone-sized screen with simulated readings."></td>
<td width="70%"><img src="docs/images/system-info.png" alt="Device Diagnostics page showing firmware and Modbus library version with simulated device values."></td>
</tr>
<tr>
<td><em>Mobile layout (simulated).</em></td>
<td><em>Device Diagnostics shows the installed Modbus library version (simulated).</em></td>
</tr>
</table>

## Quick start

### Downloads

Most users only need the [web installer](#install-on-your-esp32-in-4-steps).

| Download | For | Where |
| --- | --- | --- |
| Gateway firmware, new board | First install or recovery | [Web installer](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/), or `factory-<board>.bin` at `0x0` from the [releases](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases) |
| Gateway firmware, update | A gateway that already runs this firmware | **Settings > GitHub Firmware Updates**, or `firmware-<board>.bin` on the [latest release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/latest) |
| Wilcoxon883MRTU library | Using the Modbus RTU/TCP library in your own sketch | `Wilcoxon883MRTU-<version>.zip` on the newest `wilcoxon883mrtu-v*` [release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases); in Arduino IDE use **Sketch > Include Library > Add .ZIP Library** |

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
3. Connect the board over USB and run, choosing the environment for your board:

```sh
pio run -e adafruit_feather_esp32s3            # or: -e cytron_maker_esp32 / -e cytron_iriv_io_controller
pio run -e adafruit_feather_esp32s3 --target upload
pio device monitor
```

Each [supported board](#supported-boards) has its own environment, `adafruit_feather_esp32s3`, `cytron_maker_esp32` or `cytron_iriv_io_controller`, using the Arduino framework. `pio run` alone builds all three. The IRIV build uses the Arduino-Pico core through the community `maxgerhardt/platform-raspberrypi` PlatformIO platform, pinned to a fixed commit; its `firmware.uf2` is in `.pio/build/cytron_iriv_io_controller/`. PlatformIO installs the libraries declared in [platformio.ini](platformio.ini). The build helper also requires Python's `certifi` package in the PlatformIO Python environment.

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
| ESP32 board | Gateway; see [Supported boards](#supported-boards). Project hardware targets the Cytron Maker Feather AIoT S3 |
| Wilcoxon 883M | RS485 Modbus RTU vibration and temperature sensor |
| RS485-to-TTL UART module | Interface with ESP32-compatible 3.3 V logic levels |
| Suitable DC supply | Sensor power; check the linked sensor manual for supply and wiring requirements |

**Current firmware assignments** from [src/config/config.h](src/config/config.h):

| Connection / function | ESP32-S3 build | Cytron Maker ESP32 build | IRIV IO Controller build |
| --- | --- | --- | --- |
| RS485 module RO → UART RX | GPIO18 | GPIO18 | Built in (UART1 RX, GP25) |
| UART TX → RS485 module DI | GPIO17 | GPIO17 | Built in (UART1 TX, GP24) |
| RS485 module DE and RE direction control | GPIO14 | GPIO14 | Not needed: automatic |
| Buzzer | GPIO5 | GPIO26 (onboard) | GP11 (internal) |
| User button | GPIO0 | GPIO4 (onboard) | GP28 (USER) |
| Status LED | GPIO48, RGB | GPIO2, onboard single-colour LED | GP29 (USR LED) |
| Network | Wi-Fi | Wi-Fi | Ethernet, W5500 on SPI0 (GP18–23) |

Connect the sensor's RS485 pair to the module's A/B terminals according to the hardware documentation. Confirm grounding and termination for your installation. Board labels can differ from GPIO numbers; verify the selected board before applying power.

[Wilcoxon 883M user manual (PDF)](883M-User-Manual-Rev.1.0_20241206-EN.pdf)

## MQTT

The default base topic is **`883M`**, configurable in Settings. TLS is supported (TLS 1.2; the gateways cannot use TLS 1.3).

To check a broker and watch a running gateway's messages from your computer (Python, no extra packages; password from the `MQTT_PASSWORD` environment variable):

```sh
python scripts/mqtt_check.py --host <broker> --port 8883 --tls --insecure --user <user> --command get_metrics --seconds 60
```

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

| 90 | Waveform ready: 2 = X, 3 = Y, 4 = Z, 0 = cleared | — | — |
| 91 | Spectrum ready: same values as 90 | — | — |

*Displacement RMS is not currently read from the sensor. See [the register cache implementation](src/modbus/modbus_tcp.cpp) and [register constants](src/config/config.h) for the complete mapping.*

Server behaviour: function codes 03, 04, 06 and 16, up to **8 clients**, any unit ID, up to 125 registers per read. Registers 0–99 accept writes, but the next sensor update overwrites them. Waveform and spectrum registers are read-only and are copied from a finished capture, so a PLC never reads a buffer that is still downloading. A read that spans an unmapped gap (for example 95–105) returns an illegal-address exception. Modbus TCP has no authentication; keep port 502 on a trusted network.

![Modbus TCP settings page with the register map and simulated live values.](docs/images/modbus-tcp-settings.png)

*Settings > Modbus TCP lists the register map with live values (simulated here).*

> **Known gap:** that page describes **92/93 as capture triggers** for PLCs, but the firmware does not act on writes to them yet. Start captures from the dashboard or with the MQTT `trigger_capture` / `trigger_all` commands for now.

## Firmware updates

Open **Settings > GitHub Firmware Updates** to check for a newer stable release and install it on the **ESP32 gateway**.

The updater requires a stable `vMAJOR.MINOR.PATCH` release, the asset for the gateway's own board (`firmware-adafruit_feather_esp32s3.bin` or `firmware-cytron_maker_esp32.bin`), and GitHub's SHA-256 asset digest. It validates HTTPS certificates, image compatibility, partition size and the downloaded checksum before activating the image and rebooting. Wi-Fi and a correct gateway clock are required.

See the [update guide](demo/README.md#github-firmware-installation) for release publishing and validation details. Updates apply to the gateway; they do not update the Wilcoxon sensor firmware.

**Beta releases** use tags such as `v4.10.0-beta.1`. They are published as GitHub **pre-releases**, never marked Latest, so the gateway's updater does not offer them. To try a beta, download the `firmware-<board>.bin` for your board from the pre-release and upload it under **Settings > Device Diagnostics > Firmware Update (OTA)** or with PlatformIO. A gateway running a beta can later move to the matching stable release (for example `v4.10.0`) through the normal updater.

## Watchdog protection

The background task configures the ESP32 task watchdog for 60 seconds with panic handling and explicitly subscribes itself. It feeds the watchdog in its main loop and during sensor block reads and MQTT chunk publishing. This is intended to recover from a subscribed task that stops servicing the watchdog.

The web task is not explicitly subscribed, and initialization/subscription return values are currently unchecked. Full task coverage and recovery must be verified on the actual board, including healthy long captures and firmware updates. Source: [task setup](src/tasks/tasks.cpp).

## Operational notes

- Trend history (120 successful snapshots) and capture history (20 results) are stored in RAM and reset on reboot. Alarm settings persist on the real gateway.
- Dashboard alarms are disabled until configured. They do not control machinery or the buzzer; choose limits appropriate to the machine.
- Web and OTA endpoints currently have no authentication. Deploy the gateway on a trusted network.
- Firmware integrity checks do not provide signature verification or application-health rollback. Actual sensor operation and flash updates still require validation on the target hardware.

## Project layout

| Location | Contents |
| --- | --- |
| `src/config/`, `src/core/` | Pin/register definitions, shared state and monitoring |
| `src/modbus/` | Sensor RTU communication and TCP register cache |
| `experiments/Wilcoxon883MRTU-beta/` | Our Modbus RTU client and TCP server library, used by the firmware |
| `src/capture/`, `src/mqtt/` | Capture state machine, logs, publishers and commands |
| `src/web/` | Dashboard, settings pages and HTTP routes |
| `src/ota/` | Firmware upload and GitHub release installation |
| `src/hardware/`, `src/tasks/`, `src/utils/` | Hardware helpers, FreeRTOS tasks and utilities |
| `demo/` | Hardware-free preview and browser checks |
| `installer/` | Browser installer page published to GitHub Pages |
| `scripts/` | Build configuration, web installer bundle and site helpers |

## License

Released under the [MIT License](LICENSE).
