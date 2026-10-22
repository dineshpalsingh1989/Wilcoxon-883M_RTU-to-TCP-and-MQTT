"""Render public firmware download instructions without accessing GitHub."""
import sys


def notes(tag, repo):
    base = f"https://github.com/{repo}"
    assets = f"{base}/releases/download/{tag}"
    owner, project = repo.split("/", 1)
    installer = f"https://{owner}.github.io/{project}/"
    return f"""## Choose your sensor, then your board

[**Open the web installer**]({installer}) | [Source and setup]({base}/tree/{tag})

This release includes separate **Wilcoxon 883M** and **Wilcoxon ILA210** images. Never interchange them.

### Wilcoxon ILA210: public test firmware

ESP32-S3 and IRIV builds and simulated software checks pass. Physical ILA sensor/IOC validation is pending; confirm the exact sensor model/register manual and wiring before equipment use. The original ESP32/Maker ESP32 is not supported by ILA210.

| Board | First installation | Firmware file |
| --- | --- | --- |
| ESP32-S3 DevKitC-1-N8 | [Browser installer: choose ILA210, then ESP32-S3]({installer}#ila210) | [Factory image: flash at 0x0]({assets}/factory-ila210-esp32s3.bin) |
| Cytron IRIV IO Controller (RP2350) | Hold BOOT, tap RESET, release BOOT, copy UF2 to RP2350 drive | [Download ILA210 IRIV UF2]({assets}/firmware-ila210-cytron_iriv_io_controller.uf2) |

ILA210 adds separate digital-input/push-button, analog/4-20 mA, alarm-output and sensor-communication pages. MQTT publishes commanded output states and communication faults; it does not confirm a physical load operated. New input actions, analog channels and spare outputs start disabled.

[ILA210 setup and wiring]({base}/blob/{tag}/ILA210/README.md) | [Readiness and bench checks]({base}/blob/{tag}/ILA210/docs/ioc-readiness.md)

### Wilcoxon 883M

Use the installer's **Wilcoxon 883M** selection. ESP32 chips are detected automatically, but board wiring must match.

| Board | Manual first-install file |
| --- | --- |
| Adafruit Feather ESP32-S3 | [Factory image: flash at 0x0]({assets}/factory-adafruit_feather_esp32s3.bin) |
| Cytron Maker ESP32 (8 MB) | [Factory image: flash at 0x0]({assets}/factory-cytron_maker_esp32.bin) |
| Cytron IRIV IO Controller | [Copy 883M UF2 to RP2350 drive]({assets}/firmware-cytron_iriv_io_controller.uf2) |

### File types and checksums

- `factory-*.bin`: complete ESP32 first-install images, written at **0x0**; clears saved settings.
- `firmware-*.bin`: application images. Use only the matching firmware's supported update procedure; these are not complete first-install images.
- `firmware-*.uf2`: IRIV USB-drive installation. Choose the correct sensor prefix.
- `web-installer-*.zip`: browser-installer bundles; the installer handles these automatically.
- [SHA256SUMS.txt]({assets}/SHA256SUMS.txt): checksums for every published firmware image and installer bundle.

IRIV uses wired Ethernet and plain MQTT on port 1883. ESP32 uses Wi-Fi. Physical installation and output acceptance remain separate from successful software checks.
"""


if __name__ == "__main__":
    print(notes(sys.argv[1], sys.argv[2]))
