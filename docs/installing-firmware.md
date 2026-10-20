# Installing the gateway firmware

There are three ways to put the gateway firmware on an **Adafruit Feather ESP32-S3**. Use the first one unless you are developing the firmware.

| Method | Needs | Keeps settings |
| --- | --- | --- |
| [Web installer](#web-installer) | Chrome or Edge on a computer, USB-C data cable | Yes, unless you tick *Erase device* |
| [Manual flash with esptool](#manual-flash-with-esptool) | Python and `esptool` | Yes |
| [Build from source](../README.md#build-and-flash-the-gateway) | PlatformIO | Yes |

A gateway that is already running and on Wi-Fi can update itself under **Settings > GitHub Firmware Updates**; see [Firmware updates](../README.md#firmware-updates).

## Web installer

1. Open **https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/** in Chrome or Edge on Windows, macOS, Linux or ChromeOS. Firefox, Safari and phones cannot flash over USB.
2. Plug the Feather in with a USB-C **data** cable.
3. Click **Install** next to the version you want and pick the board's port.
   - **Stable** is the Latest release and is recommended.
   - **Beta** appears only when a newer beta pre-release exists. Use it for testing.
4. Leave *Erase device* unticked to keep saved Wi-Fi, MQTT and alarm settings. Tick it for a clean first install or if the gateway misbehaves after an update.
5. When it finishes, press **RESET** on the board, then [connect to Wi-Fi](../README.md#connect-to-wi-fi).

**No port appears, or the install fails to connect:** put the board into download mode. Hold **BOOT**, tap **RESET**, release **BOOT**, then click **Install** again. On Windows, close the Arduino IDE, PlatformIO monitor or any other program holding the port.

The installer writes the same images, at the same offsets, as `pio run --target upload`:

| Offset | File | Purpose |
| --- | --- | --- |
| `0x000000` | `bootloader-tinyuf2.bin` | Second-stage bootloader |
| `0x008000` | `partitions.bin` | Partition table (two 1408 KB OTA slots) |
| `0x00e000` | `boot_app0.bin` | Selects the first OTA slot |
| `0x010000` | `firmware.bin` | Gateway application |
| `0x2d0000` | `tinyuf2.bin` | Adafruit UF2 bootloader |

## Manual flash with esptool

Download `web-installer-adafruit_feather_esp32s3.zip` from a [release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases), unzip it, enter download mode as above and run, from the unzipped folder:

```sh
python -m esptool --chip esp32s3 write_flash \
  0x0 bootloader-tinyuf2.bin 0x8000 partitions.bin 0xe000 boot_app0.bin \
  0x10000 firmware.bin 0x2d0000 tinyuf2.bin
```

Add `--port COM5` (Windows) or `--port /dev/ttyACM0` (Linux) if esptool picks the wrong port. The offsets are also listed in the bundle's `manifest.json`.

`firmware-adafruit_feather_esp32s3.bin` on its own is the application image only. Use it for **Settings > Device Diagnostics > Firmware Update (OTA)** on a running gateway, not for a blank board.

## How releases reach the installer (maintainers)

The [Release workflow](../.github/workflows/release.yml) builds each `v*` tag, attaches `firmware-adafruit_feather_esp32s3.bin` and `web-installer-adafruit_feather_esp32s3.zip`, then calls the [Web installer workflow](../.github/workflows/installer-pages.yml). That workflow rebuilds the GitHub Pages site from the Latest stable release plus any newer `v*-beta.N` pre-release. Releases published before the installer bundle existed (up to `v4.10.0-beta.1`) are skipped, so the page shows nothing until the next gateway release.

One-time repository setup:

1. **Settings > Pages > Build and deployment > Source:** choose **GitHub Actions**.
2. **Settings > Environments > github-pages > Deployment branches and tags:** add a tag rule `v*`. Without it, deployments started by release tags are rejected and only manual runs from `main` succeed.

To republish the page without a new release, run **Actions > Web installer > Run workflow** on `main`.

The installer page itself is [installer/index.html](../installer/index.html). It loads ESP Web Tools from unpkg; the firmware images are served from GitHub Pages because release downloads do not allow cross-origin browser access. Browser flashing still needs to be confirmed on a real board after the first release that includes the bundle.
