# Repository guidance for coding agents

## Project and entry points
- ESP32-S3 Arduino firmware built with PlatformIO; environment: `adafruit_feather_esp32s3`.
- `src/main.cpp` initializes the gateway. `src/core/modules.h` collects module interfaces.
- `src/config/config.h` is the source of truth for GPIOs and register constants.
- `src/modbus/` handles sensor RTU reads and the TCP cache; `src/capture/` owns capture sequencing.
- `src/web/` embeds HTML/CSS/JS in C++ raw strings. The Node demo renders those same strings.
- `src/core/monitoring.*` owns bounded monitoring history; `src/ota/` owns firmware installation.

## Working rules
- Read the relevant module before changing behavior. Keep changes focused on the requested task.
- Preserve hardware pin assignments unless explicitly asked to change wiring. Update the README and wiring diagram together when assignments change.
- Preserve mutex protection, bounded buffers and nonblocking capture/update state machines. Do not add heap-heavy UI assets to the device.
- Keep CSS and essential browser functionality local to the firmware; optional CDN icons must not block operation.
- Preserve element IDs, route names and MQTT/register contracts when styling pages.
- Do not label simulated readings as real or claim hardware validation from demo tests.
- Never commit Wi-Fi credentials, MQTT secrets, tokens, generated trust headers or build artifacts.
- Preserve certificate, checksum, image and partition validation in the updater.
- Changes to sensor protocol or electrical connections must be checked against the bundled 883M manual; do not invent connector pin numbers or cable colors.

## Verification
- Documentation: check relative links, GPIO labels and `git diff --check`.
- Embedded UI: run `node demo/smoke_test.js`, start `node demo/mock_server.js`, then run `node demo/browser_test.js` with `DEMO_URL` matching the demo port (default browser-test port is 8090).
- Browser checks require Playwright in `.tools/node_modules` and installed Chrome; see `demo/README.md`.
- Firmware: run `pio run` (or `.venv/Scripts/python.exe -m platformio run` in this workspace). For the workspace-local tool cache, set `PLATFORMIO_CORE_DIR` to `.pio-core`. Report unavailable dependencies or hardware checks plainly.
- Check desktop and mobile layouts, keyboard focus, offline state and capture controls after UI changes.

## Documentation and delivery
- Keep `README.md` focused on setup and integration; detailed guides belong in `docs/`.
- Store tracked README images in `docs/images/`; label demo screenshots as simulated.
- Summarize changes, checks performed and any hardware validation still required.

## Specialized review agents
- Reusable project agents are defined in `.codex/agents/`; see [.codex/agents/README.md](.codex/agents/README.md). Claude Code equivalents with hyphenated names live in `.claude/agents/`.
- `review_coordinator`: collect specialist reports, verify/deduplicate findings and return one prioritized report; see [coordination guide](docs/agent-coordination.md).
- `firmware_review`: firmware tasks, captures, monitoring and hardware helpers.
- `communications_review`: Modbus RTU/TCP and MQTT integration.
- `web_update_review`: dashboard/settings, HTTP handlers and firmware updates.
- `esp32_performance_review`: heap, task stacks, scheduling, watchdog coverage and flash/OTA capacity.
- `validation_review`: failure/recovery verification and separation of simulated tests from hardware evidence.
- When parallel review is requested, assign these distinct scopes, verify the evidence, and consolidate duplicate findings before reporting. Explicitly assign file ownership before parallel edits.
