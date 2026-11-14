# Private installation registry

The public installer can show and download a device startup record. To save it to GitHub, deploy the Worker in [services/install-registry](../services/install-registry/) and connect it to a **new private repository**. The Worker writes one private GitHub issue per submitted record. It does not put a GitHub credential in GitHub Pages, firmware or the record.

## Setup

1. The private [wilcoxon-install-records repository](https://github.com/dineshpalsingh1989/wilcoxon-install-records) has been created with Issues enabled. Each saved serial number record will appear in its [Issues list](https://github.com/dineshpalsingh1989/wilcoxon-install-records/issues).
2. Create a fine-grained GitHub token scoped **only** to that repository with **Issues: read and write**. [GitHub's API](https://docs.github.com/en/rest/issues/issues#create-an-issue) requires that permission to create records.
3. In `services/install-registry`, deploy `worker.js` using [Wrangler](https://developers.cloudflare.com/workers/wrangler/commands/workers/). Set Worker secrets `GITHUB_TOKEN`, `GITHUB_REPO` (`dineshpalsingh1989/wilcoxon-install-records`), and a long random `INSTALL_KEY` using [Cloudflare's secret controls](https://developers.cloudflare.com/workers/configuration/secrets/). `ALLOWED_ORIGIN` in `wrangler.jsonc` must exactly match the Pages origin.
4. In the public firmware repository, set Actions variable `INSTALL_REGISTRY_ENDPOINT` to the Worker HTTPS address plus `/installations`, then run **Actions > Web installer > Run workflow**. The generated `channels.json` exposes only this endpoint URL.
5. Enter the private installation key once on the installer page. After flashing, connect to USB logs and press RESET. When the startup report appears, the page submits it automatically and displays a link to the resulting private GitHub issue. The key stays in the open page only; it is not saved in browser storage. Give it only to people who should register installations.

The USB startup record confirms that the gateway application booted and reports the board ID, Wi-Fi or Ethernet MAC, version and available 883M identity. It does **not** prove sensor measurement accuracy, electrical wiring, relay output or that the selected image was installed by this particular browser session. A blank board or firmware without the `INSTALL_INFO` line cannot provide this record before flashing. A missing sensor serial is stored as `null` with result `gateway_booted_sensor_not_identified`.

The IRIV Ethernet MAC and RP2350 board ID are separate values. Keep both in the record so a changed MAC configuration does not confuse two physical controllers. For ESP32, the Wi-Fi MAC and chip-derived board ID are recorded.

## Verification

Test the Worker with a sample record before using real device identifiers. Confirm that a private issue is created, incorrect keys are rejected, and requests from other origins are rejected. Then validate a real ESP32 and IRIV on a bench. The installer cannot read the IRIV USB bootloader drive in this flow, so IRIV registration follows its UF2 copy and reboot.
