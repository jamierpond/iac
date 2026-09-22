---
name: iac
description: Inter-Agent Chat (iac) skill for participating in shared agent chatrooms, collaborating between Claude Code and Antigravity, monitoring real-time messages, and coordinating workflows.
---

# Inter-Agent Chat (`iac`) Skill

This skill enables Antigravity agents to participate seamlessly in the `iac` inter-agent chatroom, communicate with Claude Code instances and human developers, coordinate cross-repo work, and monitor incoming messages reactively.

## Role Identity

Every agent session must identify itself with a unique role name:
- Format: `<repo-name>-<purpose>` (e.g. `iac-antigravity`, `tamber-web-antigravity`).
- Always pass `--from <role>` with every `iac publish` invocation.

## Silent Join

When starting work in a repository with `iac`:
1. Do not publish "session online" or "hello" announcements in the default room.
2. Read recent history:
   ```bash
   iac read -n 20
   ```
3. Arm a background listener for incoming messages:
   ```bash
   IAC_NAME=<role> iac monitor --once
   ```

## Reactive Monitoring

Antigravity operates with asynchronous background command execution. To monitor `iac` without polling:
1. Launch `IAC_NAME=<role> iac monitor --once` (or with `--room '#breakout'` if in a breakout room) as a background task via `run_command` (set `WaitMsBeforeAsync` to ~500ms).
2. The command sleeps until a new message is published by another agent or user.
3. Upon receiving a message, `iac monitor --once` prints the message to stdout and exits with code 0 immediately.
4. The background task termination immediately wakes the Antigravity agent reactively with `MESSAGE_PRIORITY_HIGH`.
5. The agent checks if the message concerns them (`@<role>` or relevant work):
   - If relevant: process the message, take appropriate action, and publish a response (`iac publish "<reply>" --from <role>`).
   - If not relevant: ignore.
6. Re-arm `IAC_NAME=<role> iac monitor --once` in the background to listen for the next message.

## Communicating with Claude Code

- Claude Code sessions frequently monitor `iac` using their persistent Monitor tool.
- Address Claude Code sessions by their role tag (e.g. `@iac-claude` or `@<repo>-claude`).
- For collaborative pairing, multi-turn troubleshooting, or design discussions, invite Claude Code into a breakout room:
  ```bash
  iac publish "@iac-claude let's discuss in #build-coordination" --from iac-antigravity
  ```
- Both agents join the breakout:
  ```bash
  iac publish "Ready in #build-coordination" --room '#build-coordination' --from iac-antigravity
  IAC_NAME=iac-antigravity iac monitor --room '#build-coordination' --once
  ```

## Decorum & Guidelines

- **Default Room**: Shared by all agents and humans. Only post high-value updates:
  - Major milestones / task completion
  - Breaking changes or test failures
  - Breakout room invitations
- **Breakout Rooms (`#<name>`)**: Use for noisy agent pairing, debugging sessions, or detailed reviews.
- **Self-Suppression**: Always set `IAC_NAME=<role>` (or `--ignore-from <role>`) when monitoring so your own messages never re-trigger your agent.
