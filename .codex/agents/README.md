# Project review agents

Six project-scoped Codex agents cover firmware, communications, web updates, ESP32 performance and validation:

| Agent | Scope |
| --- | --- |
| `review_coordinator` | Collect reports, verify evidence, remove duplicate findings and summarize coverage |
| `firmware_review` | FreeRTOS, captures, shared state, monitoring, hardware helpers |
| `communications_review` | Modbus RTU/TCP, MQTT, scaling, chunking, TLS and topics |
| `web_update_review` | Embedded UI, HTTP handlers, exports, OTA and GitHub updates |
| `esp32_performance_review` | Heap, task stacks, scheduling, watchdog coverage and OTA partition capacity |
| `validation_review` | Failure/recovery tests, data integrity and separation of demo/build/hardware evidence |

Each TOML file defines one reusable agent. Models and reasoning settings inherit from the parent session. Agents review without editing unless explicitly assigned a fix.

Example request:

> Use firmware_review, communications_review and web_update_review to review this repository in parallel. Verify their findings and return one report ordered by severity. Do not change code.

For implementation, assign the affected files and validation requirements explicitly so agents working in the shared workspace do not overlap edits.

These files define future agent behavior; they do not store running sessions or automatically fix previous findings. If the current session does not list the new agents, start a new session in this project. Discovery depends on the client supporting project-scoped custom agents.

Format reference: [official OpenAI custom-agent documentation](https://learn.chatgpt.com/docs/agent-configuration/subagents#custom-agents).

For ESP32 reliability reviews, request `esp32_performance_review` and `validation_review`. Assign reviews in batches when the session has fewer available agent slots than roles.

To gather the team reports into one answer, request:

> Use review_coordinator to collect the specialist reviews, verify their evidence, remove duplicates and give me one report. Do not change source code.

See [the coordination guide](../../docs/agent-coordination.md) for Claude usage and sharing reports across both tools.
