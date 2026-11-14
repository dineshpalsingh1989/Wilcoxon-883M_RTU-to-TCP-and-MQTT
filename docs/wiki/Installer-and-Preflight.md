# Installer and preflight

## ESP32 boards

1. Select the release in the [web installer](https://dineshpalsingh1989.github.io/Wilcoxon-883M_RTU-to-TCP-and-MQTT/).
2. Connect the board with a USB data cable and choose **Check USB and firmware**.
3. Select its serial port. The page reads the chip family and flash size, checks that the selected firmware fits, and verifies SHA-256 checksums for the image parts.
4. Confirm the printed board name and enter the device and company names. Click **Install** and select the **same port** when the install dialog asks again.
5. After the board reboots, use **Check the running gateway over USB** and press RESET. The startup report provides the gateway identity. A disconnected 883M does not prevent installation.

Supported builds are Adafruit Feather ESP32-S3 (ESP32-S3, 4 MB flash) and Cytron Maker ESP32 (ESP32, 8 MB flash). A matching chip alone cannot prove the exact board model or wiring. The second USB port selection is not automatically tied to the checked port.

The check rejects unsupported chips, unreadable or insufficient flash, images larger than flash, and checksum failures. **Erase settings and retry** is appropriate only if flash capacity is sufficient and saved settings are suspected to cause the failure. Erasing cannot add flash or fix wiring. See the [full installation guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/feature/installer-preflight/docs/installing-firmware.md).

## IRIV IO Controller

The IRIV uses an RP2350A and a manual UF2 copy. Select IRIV, confirm its printed name, enter device and company names, and download the UF2. Connect its PROG USB-C port, enter BOOT mode, and copy the UF2 to the RP2350 drive. After reboot, read the startup report over USB to obtain the board ID and Ethernet MAC. The website cannot inspect the physical IRIV board before the UF2 copy or prove the application booted until that report appears.

## What the check proves

The ESP32 check proves that the selected USB device responds as a supported chip with enough reported flash for a checksum-verified image. The post-install startup report proves that the gateway application booted and reported its identity. Neither check proves sensor accuracy, 4–20 mA wiring, relay operation, or that a different port was not chosen at the second prompt.
