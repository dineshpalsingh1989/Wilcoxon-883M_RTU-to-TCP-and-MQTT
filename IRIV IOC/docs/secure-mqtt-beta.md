# Wilcoxon 883M IRIV IOC secure MQTT beta

Local firmware version: **4.10.0-beta.3**. Target: Cytron IRIV IO Controller, RP2350 + W5500, Wilcoxon **883M** sensor. This is the full gateway, including dashboard, capture, Modbus RTU/TCP and MQTT. Hardware validation is pending.

## Files and installation

The prepared package is in the workspace's ignored `.pio/beta-packages/883m-iriv-secure-mqtt-4.10.0-beta.3/` directory:

- `firmware-cytron_iriv_io_controller.uf2`: install by copying to the board's USB bootloader volume after entering its USB bootloader mode.
- `firmware-cytron_iriv_io_controller.bin`: manual upload through the existing gateway's Device Diagnostics firmware upload control.
- `SHA256SUMS.txt`: checksums for the two firmware files.

Use the board's documented USB bootloader procedure. Uploading replaces the running gateway application. The UF2 contains program-flash blocks only, below the filesystem region; it does not include a filesystem erase image. Back up important configuration before a bench upload. Keep power connected during installation. This package has not been published to GitHub, and the IOC does not support direct GitHub installation through the dashboard.

## Configure secure MQTT

1. Connect Ethernet to a network providing DHCP, DNS and access to the broker. Open the gateway's assigned IP address. Without DHCP it falls back to `192.168.4.1`, which is for local configuration and has no default gateway for reaching an internet broker.
2. Open **Settings > MQTT Settings**. Enter the broker's DNS hostname, port **8883**, credentials if required, and base topic. Enable MQTT and TLS.
3. For verified TLS, paste the broker's **public root CA certificate in PEM format** into the certificate box. To connect without a CA, enable **Skip Certificate Validation** instead and leave the certificate box empty. Use the certificate supplied by your broker/provider. Up to 8192 bytes and 8 certificates are accepted. Private keys are rejected. Leave the box blank on later edits to retain the stored CA.
4. Save and reboot. MQTT settings are sent in the POST body. Configure from a trusted local network: the gateway settings page itself uses HTTP.
5. Ensure the clock is correct using NTP or **Time Settings**. The gateway waits for a plausible clock before connecting and requires a valid certificate chain and matching hostname. The status should become **Connected (verified TLS)**. Incorrect CA, hostname, certificate validity or credentials must be investigated rather than bypassing verification.

Existing settings are retained. A previously saved plain-MQTT configuration stays plain until TLS is enabled. Certificate verification is enabled by default. To use TLS without installing a CA, explicitly enable **Skip Certificate Validation** in MQTT Settings and save. Port 8883 and TLS encryption remain enabled, but the broker identity is not authenticated. The status then reads **Connected (TLS, certificate unchecked)**. Switching the option off restores certificate and time checks; install a CA before saving that mode. No credentials or broker-specific certificate are embedded in the downloadable firmware. Client-certificate authentication (mTLS) is not implemented in this beta.

## Watchdog and scheduling

From v4.12.0 the watchdog is on: a supervisor task restarts the gateway when the background task stops checking in for 60 seconds. A bench build can still turn it off with `-DGATEWAY_DISABLE_WATCHDOG=1`. `/firmware_info` reports `watchdog_enabled` and the firmware version.

HTTP, MQTT, Modbus TCP and sensor/capture steps run sequentially from Arduino's core-0 loop. A linker wrapper gives that actual Arduino task a 32 KB static stack, below the heap, to accommodate gateway calls and BearSSL's alternate stack on RP2350. This is specific to the pinned Arduino-Pico build; recheck it when updating the framework. SDK sources are not modified.

TLS handshakes are synchronous and can take seconds. New connection attempts are deferred during active captures. Handshakes and existing synchronous sensor reads/transfers can pause dashboard and Modbus responses. This beta does not promise real-time response latency. The manual updater acknowledges the shared loop's quiescent state before writing; image/staging checks remain in place.

## Verification

Build with `pio run -d "IRIV IOC"`, or use the workspace `.venv/Scripts/python.exe -m platformio run` and `PLATFORMIO_CORE_DIR=.pio-core`.

Software checks performed on 2026-10-25:

- Full RP2350 gateway compiled and linked with the pinned platform and Arduino-Pico 6.1 package.
- Linked-image inspection confirmed the Arduino task creation uses the stack wrapper (that beta build had the watchdog disabled).
- IRIV page smoke tests and desktop/mobile browser tests passed: dashboard, capture controls, offline behavior and settings.
- Focused MQTT browser tests cover keyboard switching/focus, verified-default behavior, explicit no-CA mode, CA failure preventing settings save, POST submission and offline status. HTTP responses and sensor data were simulated.

Still required on real hardware: boot and heap/stack observations; broker connection/publish/subscribe; rejection of wrong CA, expired certificate and mismatched hostname; clock recovery; Ethernet/broker reconnect; capture/web/Modbus latency; settings and CA persistence; manual firmware update and restart recovery. Compilation and browser tests do not establish these results.
