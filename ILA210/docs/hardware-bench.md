# ILA210 hardware acceptance checklist

**Status: NOT RUN on physical hardware.** Successful compilation, native fake
transport tests and simulated browser checks are not deployment acceptance.
Record board revision, sensor serial/firmware, gateway commit, wiring, power
supply, instrument models and test results before signing off either board.
Preserve board wiring from [wiring.md](wiring.md) and sensor protocol details
from [register verification](register-verification.md).

| Test | Procedure and acceptance | Evidence |
| --- | --- | --- |
| First connection | Confirm 9600/8N1/ID 1 for a factory sensor; read all selected metrics and compare with the vendor tool. | Not run |
| Search | Search known and unknown IDs/baud rates, absent sensor and wrong-format sensor. Check progress/cancel, restored connection and no sensor writes in an RS485 trace. | Not run |
| Configuration | Change ID, then baud independently. Verify settings readback and persistence after both sensor and gateway restart. | Not run |
| Interrupted changes | Disconnect after write, interrupt power before gateway save, force storage failure. Check uncertainty reporting and recovery with search; no automatic rollback writes. | Not run |
| UART integrity | Measure direction/turnaround timing, termination and bias; inject CRC errors/no reply, then reconnect. Check counters and recovery without reboot. | Not run |
| Captures | Capture 25,600 signed waveform and 6,401 unsigned spectrum points. Verify first/last addresses, scaling and values above 32767 with recorded sensor data. | Not run |
| Concurrent clients | Poll TCP with multiple PLC clients and publish MQTT while capturing/searching. Exercise slow clients, malformed frames and disconnects; confirm bounded memory and watchdog operation. | Not run |
| IRIV digital inputs | Test each configured DI and push-button, active polarity, debounce, acknowledge and capture actions. Confirm capture rejection during setup. | Not run |
| IRIV analog inputs | Match physical mode switches. Inject calibrated 4, 12 and 20 mA into each configured current input and check engineering scaling, thresholds, hysteresis and delay. | Not run |
| Analog faults | Inject supported under/over-range conditions and disconnect the loop. Check configured fault alarms and recovery. Follow the controller's electrical limits. | Not run |
| Outputs | Initially disconnect loads. Verify DO0/DO1 vibration alarms and selected loss policy; verify DO2/DO3 alarm sources, latch/acknowledge and power-up states before connecting controlled loads. | Not run |
| Search alarm behavior | Run a scan longer than the loss timeout. Verify configured loss behavior, continued MQTT/TCP servicing and normal alarm recovery after polling resumes. | Not run |
| Networking/recovery | Save a static IP and confirm the gateway answers there after restart; switch back to DHCP. Exercise DHCP failure, Ethernet cable removal/reconnect, broker unavailable/recovery and USER-button fallback. Confirm saved settings survive restart. | Not run |
| Soak | Run the intended production traffic and capture interval, recording heap, task/watchdog behavior, missed messages and recovery events. Define acceptable alarm latency for the application and measure it under worst-case traffic. | Not run |

ESP32-S3 has no IOC onboard 4–20 mA/DI/DO hardware; mark those rows not
applicable there. Existing MQTT I/O topics and TCP register contracts remain
unchanged. Record actual MQTT output-state transitions alongside physical
measurements; a simulated state message alone does not prove an output switched.

Deployment approval: **pending**. Tester/date/results: **not recorded**.
