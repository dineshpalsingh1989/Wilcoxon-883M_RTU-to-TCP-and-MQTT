# Installation and validation project plan

This plan tracks work needed to publish the installer preflight and private installation registry. The implementation is in [draft PR #16](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/pull/16).

| Work item | Done when |
| --- | --- |
| [ESP32 installation bench validation](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/17) | Feather ESP32-S3 and Maker ESP32 pass physical preflight, install, reboot and identity checks; wrong chip, insufficient flash, changed port and checksum failure are checked. |
| [IRIV installation bench validation](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/18) | Physical UF2 copy, reboot, board ID, Ethernet MAC and gateway serial are observed; connected and disconnected 883M states are checked. |
| [PCC420V and DI/DO validation](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/19) | Analog scaling, loop fault, dashboard visibility and DI/DO rules pass physical tests. |
| [Private registry deployment](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/issues/20) | Worker is deployed with secrets, installer endpoint is configured, and a sample record reaches the private repository while wrong key and origin are rejected. |
| Publish wiki and project board | Initialize the GitHub Wiki, copy these pages, and add the linked work items to a GitHub Project after Project API access is granted. |

Keep customer and device records in the [private registry repository](https://github.com/dineshpalsingh1989/wilcoxon-install-records/issues), never in this public plan.
