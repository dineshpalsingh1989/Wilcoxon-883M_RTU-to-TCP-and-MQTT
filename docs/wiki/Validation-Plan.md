# Validation plan

## Automated evidence

The draft [installer preflight PR #16](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/pull/16) includes installer browser tests, installation registry tests, installer generation tests, and firmware CI. These establish that the code builds and the simulated browser flows behave as expected. They are not a substitute for physical board tests.

## Bench checks before release

- [ESP32-S3 and Maker ESP32](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/17): use actual USB ports to check chip/flash detection, image verification, rejection cases, second-port selection, install, reboot and identity report.
- [IRIV](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/18): copy UF2 to a real RP2350 drive, reboot, and confirm its board ID, Ethernet MAC and gateway serial over USB.
- Test with and without an 883M. Confirm that an absent sensor is recorded as missing while the gateway remains installable.
- [Connect a PCC420V](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/19) through the approved wiring. Confirm raw current, scale endpoints, model selection, loop fault, enabled/disabled dashboard cards, and configured DI/DO behavior.
- [Deploy the private registry Worker](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/20), submit a sample record, then verify authenticated success, wrong-key rejection, origin rejection, and private issue visibility before storing real identifiers.

Keep the draft PR and documentation status aligned with the bench results. Record failures and fixes in repository issues; keep actual installation data in the private registry.
