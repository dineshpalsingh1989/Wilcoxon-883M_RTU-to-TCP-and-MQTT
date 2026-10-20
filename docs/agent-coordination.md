# Agent coordination

The coordinator gathers specialist reports and returns one answer with verified findings, checks performed, decisions and outstanding work.

| Tool | Coordinator | Specialist agents |
| --- | --- | --- |
| Codex | `review_coordinator` | `firmware_review`, `communications_review`, `web_update_review`, `esp32_performance_review`, `validation_review` |
| Claude Code | `review-coordinator` | `firmware-review`, `communications-review`, `web-update-review`, `esp32-performance-review`, `validation-review` |

Both tools use the same five review scopes; Claude names use hyphens and Codex names use underscores. Claude Code also has fault-focused debuggers (`firmware-debugger`, `modbus-debugger`, `mqtt-debugger`) for investigating a specific crash, sensor read or broker problem.

## In VS Code

In Codex, send:

> Use review_coordinator to gather reviews of the beta RTU library from the relevant specialists. Verify their evidence, remove duplicate findings and give me one report. Do not change source code.

In Claude Code, send:

> Use review-coordinator to gather communications-review and validation-review findings for the beta RTU library. Verify them and return one report. Do not change source code.

Agents are selected by name in a supporting client. Start a new session if the client does not list the newly added coordinator. If the session cannot delegate from the coordinator, the parent can launch the specialists and pass their reports to it. The coordinator reports missing coverage rather than pretending an agent ran.

## Collect reports from both tools

These definitions do not connect the two VS Code chat windows. Choose one coordinator for the final report and give it the other tool's saved report, or paste that report into its conversation.

For a shared local handoff, ask each tool:

> Save your review to .agent-reports/rtu-review/<your-agent-name>.md using docs/agent-report-template.md. Include the reviewed revision and scope. Do not change source code.

Then ask the chosen coordinator:

> Collect the reports in .agent-reports/rtu-review/, compare their evidence with the current source and return one prioritized summary. Save it as .agent-reports/rtu-review/summary.md.

The folder is ignored by Git and is created only when reports are saved. Use a new run ID for another revision, and one filename per producer to avoid overwriting reports. Do not include credentials in handoffs. Model choice and reasoning settings remain inherited from the active session.

## Automatic bridge

[`scripts/agent_bridge.js`](../scripts/agent_bridge.js) waits for both tools to finish, then starts the coordinator. It needs Node and the `claude` CLI only. Codex can be run from the VS Code extension.

1. Start a run. The script records the commit and prints the prompt to send to each tool:

   ```sh
   node scripts/agent_bridge.js start rtu-review-1 --scope "beta RTU library"
   ```

2. Paste the printed prompts into Codex and Claude. Each tool saves `.agent-reports/rtu-review-1/codex.md` or `claude.md` when it is done.
3. In a terminal, wait and summarize:

   ```sh
   node scripts/agent_bridge.js wait rtu-review-1 --summarize
   ```

A report counts as done once its `- Status:` line reads `complete`, `partial` or `failed`. When every expected report is done, the script runs the Claude `review-coordinator` read-only (no edits, no new agents, no shell) and saves its answer as `summary.md`. After `--timeout` minutes (default 120) it summarizes whatever has finished, lists the missing producers as gaps, and exits with code 2.

- Use `status <run-id>` to check progress without waiting.
- Use `--expect codex,claude,<name>` to add producers.
- Leave out `--summarize` to have the script print the coordinator prompt so you can paste it into either tool.

The script warns when HEAD moves during a run or when a report doesn't mention the run's commit.

## What the coordinator returns

- Review coverage and agent completion status, including missing reports.
- Verified findings ordered by severity, with duplicate root causes combined and disagreements called out.
- Checks actually performed and the distinction between simulated, compiled and physical-hardware evidence.
- Reported changed files, decisions, remaining questions and the next recommended action.

The coordinator collects and verifies reports. Implementing fixes, running physical tests, flashing boards or publishing changes remains a separate task. Existing reports are not a substitute for reviewing a later code revision.
