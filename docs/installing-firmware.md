# Installing the gateway firmware

The firmware is built for the **Adafruit Feather ESP32-S3** and already contains everything the gateway needs, including the Wilcoxon883MRTU Modbus RTU/TCP library. Use the web installer unless it does not work for you; every other method installs the same images.

| Method | Needs | Keeps settings |
| --- | --- | --- |
| [1. Web installer](#1-web-installer) (recommended) | Chrome or Edge on a computer | Yes, unless you tick *Erase device* |
| [2. Espressif browser flasher](#2-espressif-browser-flasher) | Chrome or Edge, one downloaded file | No |
| [3. esptool command line](#3-esptool-command-line) | Python | Yes with the separate files, no with the factory file |
| [4. Espressif Flash Download Tool](#4-espressif-flash-download-tool-windows) | Windows | No |
| [5. Build from source](../README.md#build-and-flash-the-gateway) | PlatformIO | Yes |
| [6. Update a running gateway](#6-update-a-running-gateway) | Gateway on Wi-Fi | Yes |

Re-installing is always safe. The ESP32-S3's built-in loader is in ROM and cannot be overwritten, so a failed or interrupted install can simply be repeated from [download mode](#download-mode).

## Release files

Each gateway [release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases) has three files (the factory file from `v4.10.0-beta.3` on):

| File | Use it for |
| --- | --- |
| `factory-adafruit_feather_esp32s3.bin` | Manual install on a new or broken board. **One file, written at address `0x0`.** Clears saved settings. |
| `web-installer-adafruit_feather_esp32s3.zip` | The separate images (table below) used by the web installer, for tools that should keep settings |
| `firmware-adafruit_feather_esp32s3.bin` | Updating a gateway that already runs this firmware (OTA). **Not** for a blank board. |

The separate images in the zip, and where each one goes:

| Address | File | Purpose |
| --- | --- | --- |
| `0x0` | `bootloader-tinyuf2.bin` | Second-stage bootloader |
| `0x8000` | `partitions.bin` | Partition table (two 1408 KB OTA slots) |
| `0xe000` | `boot_app0.bin` | Selects the first OTA slot |
| `0x10000` | `firmware.bin` | Gateway application, including the Modbus library |
| `0x2d0000` | `tinyuf2.bin` | Adafruit UF2 bootloader |

`factory-adafruit_feather_esp32s3.bin` is these five images merged into one file, with the gaps filled with `0xFF`. That filling is why it clears saved Wi-Fi, MQTT and alarm settings.

## Download mode

Most tools switch the board into download mode automatically. If a tool cannot find or connect to the board:

1. Hold **BOOT**.
2. Tap **RESET**.
3. Release **BOOT**.
4. Connect again. The board may now appear as a different port.

After installing in download mode, press **RESET** to start the firmware.

## 1. Web installer

1. Open **https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/** in Chrome or Edge on Windows, macOS, Linux or ChromeOS.
2. Plug the Feather in with a USB-C **data** cable.
3. Click **Install** next to the version you want and pick the board's port.
   - **Stable** is the Latest release and is recommended.
   - **Beta** appears only when a newer beta pre-release exists. Use it for testing.
4. Leave *Erase device* unticked to keep saved settings. Tick it for a clean first install or if the gateway misbehaves.
5. When it finishes, press **RESET**, then [connect to Wi-Fi](../README.md#connect-to-wi-fi).

## 2. Espressif browser flasher

Use this if the web installer page fails but Chrome or Edge works.

1. Download `factory-adafruit_feather_esp32s3.bin` from the release.
2. Open **https://espressif.github.io/esptool-js/** and click **Connect**, then choose the board's port.
3. Under *Flash Address* enter `0x0`, choose the factory file and click **Program**.
4. When it finishes, press **RESET**.

## 3. esptool command line

Works in any terminal, without a browser. Install esptool once with `pip install esptool`, enter download mode if needed, then run one of these.

New board, or clear everything:

```sh
python -m esptool --chip esp32s3 write_flash 0x0 factory-adafruit_feather_esp32s3.bin
```

Keep saved settings: unzip `web-installer-adafruit_feather_esp32s3.zip` and run, from the unzipped folder:

```sh
python -m esptool --chip esp32s3 write_flash \
  0x0 bootloader-tinyuf2.bin 0x8000 partitions.bin 0xe000 boot_app0.bin \
  0x10000 firmware.bin 0x2d0000 tinyuf2.bin
```

Add `--port COM5` (Windows) or `--port /dev/ttyACM0` (Linux) if esptool picks the wrong port.

## 4. Espressif Flash Download Tool (Windows)

1. Download the **Flash Download Tool** from Espressif's [support tools page](https://www.espressif.com/en/support/download/other-tools) and unzip it.
2. Start it and choose **ChipType: ESP32-S3**, **WorkMode: Develop**, **LoadMode: USB**.
3. In the first row tick the box, choose `factory-adafruit_feather_esp32s3.bin` and enter `0x0` after the `@`.
4. Choose the board's **COM** port and click **START**. Use [download mode](#download-mode) if it stays on *Sync*.
5. When it shows **FINISH**, press **RESET**.

## 6. Update a running gateway

A gateway already on Wi-Fi does not need USB:

- **Settings > GitHub Firmware Updates** installs the latest stable release from GitHub.
- **Settings > Device Diagnostics > Firmware Update (OTA)** accepts `firmware-adafruit_feather_esp32s3.bin` uploaded from your computer, for example to try a beta.

Both keep saved settings. See [Firmware updates](../README.md#firmware-updates).

## Troubleshooting

| Problem | Fix |
| --- | --- |
| No port appears | Use a USB-C **data** cable (charge-only cables show no port), try another USB socket without a hub, then use [download mode](#download-mode) |
| "Failed to connect", timeout or stuck on *Sync* | Close the Arduino IDE, PlatformIO monitor or any serial terminal using the port, then use [download mode](#download-mode) |
| The port disappears or the install stops halfway | Replace the cable, avoid USB hubs, enter download mode and install again. The board cannot be bricked this way. |
| Install finishes but nothing happens | Press **RESET**. If the `ILA_Sensor_AP` network still does not appear, reinstall with *Erase device* ticked or use the factory file. |
| "Your browser does not support" | Use Chrome or Edge on a computer, or [esptool](#3-esptool-command-line) |
| Windows shows an unknown device | Windows 10 and 11 include the needed USB driver. Try another cable or socket, then download mode. |
| Gateway worked before and now misbehaves after an update | Reinstall with *Erase device* ticked, then enter Wi-Fi settings again |

## How releases reach the installer (maintainers)

The [Release workflow](../.github/workflows/release.yml) builds each `v*` tag, attaches the three release files and adds install instructions above GitHub's generated release notes. It then calls the [Web installer workflow](../.github/workflows/installer-pages.yml), which rebuilds the GitHub Pages site from the Latest stable release plus any newer `v*-beta.N` pre-release. Releases without the installer bundle (up to `v4.10.0-beta.1`) are skipped.

Repository setup (already done):

1. **Settings > Pages > Build and deployment > Source:** **GitHub Actions**.
2. **Settings > Environments > github-pages > Deployment branches and tags:** `main` and the tag rule `v*`.

To republish the page without a new release, run **Actions > Web installer > Run workflow** on `main`.

The installer page itself is [installer/index.html](../installer/index.html). It loads ESP Web Tools from unpkg. The firmware images are served from GitHub Pages because release downloads do not allow cross-origin browser access.
