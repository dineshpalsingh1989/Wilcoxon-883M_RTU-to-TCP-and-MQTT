# Sensor communication and settings

Open Settings > Sensor communication alarm (`/sensor_fault_settings`) on either firmware target. The default loss timeout is 10,000 ms; the permitted range is 2,000–600,000 ms. A successful priority velocity read resets the timer. A sensor absent at startup faults after the same timeout.

The IOC combined alarm includes communication loss by default. Disable that source separately if required; the raw fault remains visible. The independent IOC sampler evaluates the last-success timestamp, so an RTU timeout does not stop DO2/DO3 fault detection. Alarm source bit 18 is reserved for communication loss. Existing vibration alert/alarm bits remain 16/17.

Choose DO0/DO1 behavior on loss:

| Mode | Behavior |
| --- | --- |
| Hold (default) | Keep the last output state |
| Alarm ON | Turn DO1 on if its relay setting is enabled; hold DO0 |
| Both OFF | Turn DO0 and DO1 off |

Disabled relay outputs remain off. On the first successful alarm read, normal vibration threshold/dwell control resumes. An already latched IOC alarm still follows its acknowledgement rules. The separate communication-fault checkbox controls the combined IOC source, not the DO0/DO1 policy.

Priority alarm reads are scheduled every 500 ms, including during waveform/spectrum captures. Other metrics read one register per background pass; a failed metric round backs off. These are software scheduling targets, not measured timing guarantees: serial timeouts, MQTT connections, flash writes and other processing can delay service. DO0/DO1 policies run in the background task; DO2/DO3 use the independent sampler.

`GET /api/sensor_health` returns `gateway_id`, `timeout_ms`, `fault_alarm`, `output_mode`, `sensor_fault` and `sample_seen`. POST the three configuration fields as typed JSON. Invalid input returns 400; failed storage returns 503 without applying the candidate. Settings saves stage an entire namespace and commit before runtime changes or reboot. ESP32 uses one NVS snapshot; IRIV verifies a temporary JSON file before atomic rename. Old settings are read during migration. HTTP and MQTT capture interval changes serialize their save/apply operations.

MQTT commands must include the exact hardware-derived `gateway_id`. Device Info and this page show it; sensor telemetry also includes it. Serial-only commands are ignored. The sensor serial is unknown until verified against its documentation and is reported as an empty string.

Before connecting loads, test disconnection at idle and during a full capture, timeout, all output modes, disabled relays, acknowledgement, reconnection, reboot and failed storage. Software tests and builds do not establish hardware response time or electrical suitability.
