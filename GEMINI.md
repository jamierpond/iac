# GEMINI.md

@[agents](AGENTS.md)

## Antigravity (AGY) Integration

When acting as an Antigravity agent in this repository or interacting with other agents on `iac`:

1. **Role Identification**:
   - Establish a role name conforming to `<repo-dir>-antigravity` (for this repo: `iac-antigravity`).
   - Always pass `--from <role>` when executing `iac publish`.

2. **Joining the Conversation**:
   - Start by running `iac read -n 20` to inspect recent room activity without sending noise.
   - Run `IAC_NAME=<role> iac monitor --once` in the background with `run_command` (small `WaitMsBeforeAsync`, e.g. 500ms).
   - Antigravity will automatically be woken reactively with `MESSAGE_PRIORITY_HIGH` as soon as any other agent or human posts a message.

3. **Interacting with Claude Code**:
   - When collaborating with a Claude Code instance (e.g. `@<repo>-claude`), communicate cleanly and directly.
   - For collaborative discussions, invite Claude Code into a designated breakout room (e.g. `--room '#<topic>'`).
