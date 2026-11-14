# Gateway identity and private registry

Each gateway derives a repeatable `gateway_serial` from its board identity. ESP32 records use a `GW-ESP32-...` prefix; IRIV records use `GW-IRIV-...`. The generated serial identifies the gateway and is separate from the optional Wilcoxon 883M sensor serial. It is not a manufacturer-issued serial number. A different physical board produces a different board-derived value.

After installation, the USB `INSTALL_INFO` report and `/firmware_info` expose the gateway serial, board ID, MAC address, and firmware version. ESP32 records carry its Wi-Fi MAC and chip-derived board ID. IRIV records carry its Ethernet MAC and RP2350 board ID; retain both because they are distinct identifiers. The installer adds the entered device and company names to the record. Those names are not hard-coded into a shared firmware image.

If no 883M is connected or identified, installation may continue. The sensor serial is stored as `null` with result `gateway_booted_sensor_not_identified`. This state must never be shown as a successful sensor measurement.

## Automatic private storage

The installer page can download the JSON record. Automatic storage uses a Cloudflare Worker with a private GitHub credential and an installation key. Once the Worker is deployed and configured, the page submits the record after it detects the USB startup report and displays the resulting private issue link. Records are written to [wilcoxon-install-records/issues](https://github.com/dineshpalsingh1989/wilcoxon-install-records/issues), which is a **private** repository. Users without access will not be able to open that link.

The Worker is a template awaiting deployment. Until it is deployed and the installer endpoint is configured, no automatic GitHub record is created. Follow the [registry setup guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/feature/installer-preflight/docs/install-registry.md). Do not put installation keys, GitHub tokens, customer names, or real device identifiers in this public wiki.
