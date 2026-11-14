# IOC deployment review procedure

Use `ioc_readiness_review` to determine whether the ILA firmware is ready for an IRIV bench upload. This is an evidence-based review, not an automatic hardware deployment.

## Scope and working rules

Firmware is now board-owned. Use `ILA210/IRIV IOC/` as the IOC firmware project (or `IRIV IOC/` from the ILA root). Its `src/`, `lib/`, `scripts/`, `platformio.ini` and `profile.ini` contain the implementation. Also inspect `ESP32 Family/ESP32-S3/` when a fix must apply to both boards. The sensor-root `src/` contains only a navigation note; it is not compiled. Tests and demo tools remain at the sensor root.

- Review the ILA project only. The parent 883M project can supply context, but do not change its files or assume its sensor manual applies to ILA.
- Read `platformio.ini`, `src/main.cpp`, all `src/platform/`, `src/config/` and `src/ioc/` files, relevant installed dependency code, `demo/`, `tests/`, and the wiring/review/setup documentation.
- Trace each finding through its callers. Verify API assumptions and platform differences against the pinned, installed library/core rather than relying on generic ESP32 behavior.
- Report source-confirmed defects separately from hypotheses requiring hardware. Do not fill missing ILA register documentation with 883M assumptions. Preserve GPIO, route, MQTT and register contracts.
- Reviews are read-only. Any assigned implementation must state file ownership and coexist with other edits. Do not spawn additional agents unless requested.

## Required checks

1. **Boot and image:** RP2350 board identity, Arduino-Pico/FreeRTOS options, flash/filesystem layout, correct upload artifact, initialization order, task creation, static memory, stack units/core affinity and watchdog/reset behavior. Flag execution-context restrictions on flash writes and filesystem initialization.
2. **Ethernet:** W5500 SPI pin ownership, reset/CS, DHCP timing, fallback routing, link loss/recovery, USER button, network-task/core requirements and concurrent HTTP/MQTT/PLC traffic.
3. **Sensor transport:** correct UART1/Serial2 mapping and automatic RS485 direction, ModbusMaster buffer/frame limits, error paths, capture trigger/poll/read sequencing, data signedness/scaling and failed/partial read visibility. Identify missing manual evidence explicitly.
4. **Physical I/O:** DI/DO/ADC board pins, active polarity, input debounce, held-at-boot suppression, threshold/hysteresis/delay boundaries and timer rollover. Verify DO0/DO1 and DO2/DO3 ownership, output behavior at boot/config change/restart/sensor loss, combined sources, acknowledgement/re-alarm and analog physical mode/calibration/fault detection.
5. **Concurrency and storage:** mutex lock ordering, cross-task state snapshots, acknowledgement/capture queues, allocation failure, bounded JSON/buffers/logs, LittleFS access from the appropriate task/context, atomic rename and settings recovery after interruption. Check every persistence return path before a successful HTTP response/restart.
6. **Web and MQTT:** separate I/O pages, HTTP route/method contracts, stale/offline display, escaping and browser controls, certificate behavior per board, unique gateway/sensor identity, reconnect blocking and complete/non-retained waveform/status messages. Do not infer backend validity from a simulated API.
7. **Validation:** check the actual current source with firmware builds, native I/O logic tests and browser/demo tests when dependencies are available. Identify code paths not executed by those tests. Do not repeat expensive checks without a reason; coordinate validation ownership with the parent.

## Evidence and output

Return findings in priority order, using P1 high, P2 medium and P3 low. Each actionable defect needs a file and line, trigger, impact, supporting code/library evidence and a minimal proposed fix. Avoid speculative or duplicate findings.

End with:

- A verdict: **not ready for bench upload**, **ready for controlled bench upload**, or **hardware-validated**. The last verdict requires actual recorded board results.
- Which checks were performed, with exact commands/results or artifact references.
- Blockers and residual risks, including missing sensor identification/manual.
- A short first-upload and bench checklist: disconnected loads, known power/wiring, correct UF2/board, boot log, Ethernet DHCP/fallback, saved settings/reboot, known analog source, DI capture/acknowledge, beacon/sounder behavior, sensor disconnect/recovery and watchdog recovery. State which steps require explicit physical hardware authorization.

Creating this agent does not authorize flashing, resetting, power interruption or driving equipment. A readiness review may inspect builds and USB/serial discovery without opening a serial port that could reset a connected board.

Agent configuration follows the [official Codex custom-agent format](https://learn.chatgpt.com/docs/agent-configuration/subagents#custom-agents).
