# Dashboard demo

Run `node demo/mock_server.js` and open http://localhost:8080.
In PowerShell, use `$env:PORT='8090'` before starting Node to use port 8090.

The dashboard and all settings pages use the firmware's actual HTML. A demo-only
scenario selector lets you try normal operation, high vibration, and an offline
sensor. Enable alarms and set a short delay to try the alarm states. Capture a
single axis or all three axes to populate capture history. Trend and capture CSV
exports are available. Other save/reboot actions are simulated; alarm settings
last only for the demo server session. Firmware uploads are rejected.

On **Settings > GitHub Firmware Updates**, check for a demo release and select
**Simulate verified update**. Progress is simulated; no binary is installed and
no hardware is restarted.

Checks:

```text
node demo/smoke_test.js
npm.cmd install --prefix .tools --no-package-lock playwright
node demo/browser_test.js
```

The browser check uses installed Chrome on Windows. Set `CHROME_PATH` to use a
different Chromium executable, and `DEMO_URL` to change the server address.
Screenshots are written to ignored `demo/artifacts/`.

# Monitoring on the real gateway

- Sensor health counts complete metric read attempts, failed batches, response
  timeouts, consecutive failures, and age of the last successful snapshot.
- The latest 120 successful snapshots and 20 capture results live in bounded RAM
  rings. They reset on gateway reboot. Alarm settings are stored in Preferences.
- Velocity RMS thresholds apply to any X/Y/Z axis. Temperature has its own limit.
  Alarms require consecutive successful readings above a threshold for the set
  delay. Failed or stale readings reset the pending delay and report unknown.
- These are dashboard alarms; they do not drive machinery or the buzzer. Default
  limits are examples and alarms are disabled until configured for the machine.
- Capture transmission status records MQTT publish attempts or offline skips.
  It does not assert broker delivery or a PLC read.

# GitHub firmware installation

The release page checks the public repository
`dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT`; private repositories are not
supported. No personal access token is used or stored.

**Install on gateway** starts a background update task. The device re-fetches the
selected release over certificate-verified HTTPS, requires a newer stable
`vMAJOR.MINOR.PATCH` tag and the exact asset for its own board
(`firmware-<board>.bin`, where `<board>` is the `board` reported by `/firmware_info`),
and requires GitHub's SHA-256 asset digest.
It pauses sensor work, rejects overlapping captures/updates, validates that the
image header matches the gateway's chip (ESP32-S3 or ESP32) and inactive partition size, streams to the inactive slot,
and compares SHA-256 before activating the image and rebooting. Redirects are
limited to HTTPS GitHub download hosts. Failed downloads/checksums are aborted.

Wi-Fi and a correct gateway clock are required for certificate validation. The
build script embeds selected Mozilla roots from `certifi`; rebuild with updated
roots if GitHub changes its certificate chain. This implements integrity checks,
not firmware signature verification or application-health rollback after reboot.
Real sensor and flash-update behavior still needs validation on the actual board.

The release workflow builds the configured PlatformIO board. Manual runs produce
a build artifact. Pushing a stable version tag publishes the binary in a GitHub
Release and sets its firmware version from the tag. For local untagged builds,
update the default version in `src/core/globals.cpp` as appropriate. GitHub Actions
uses its temporary built-in token.

The local changes must be pushed for the workflow to exist on GitHub. Flash the
new firmware once to make these dashboard and update endpoints available on the
gateway. This work does not update the Wilcoxon sensor's own firmware.
