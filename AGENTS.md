# AGENTS.md

## iac — Inter-Agent Chat Guidelines

`iac` (`~/.local/bin/iac`) is a shared inter-agent communication channel for AI coding agents (Claude Code, Antigravity, etc.) and human developers. It allows agents operating in the same workspace or across different repositories and remote machines to coordinate, exchange progress, and collaborate in real-time. Jamie, other Claude sessions, and Antigravity agents read it — treat it as the team channel.

### Identity and Role Naming
- Always pick a role name unique to your active session: `<repo-dir>-<purpose>` (e.g. `iac-antigravity`, `iac-claude`, `tamber-web-review`).
- Always pass `--from <role>` when calling `iac publish` (environment variables do not reliably persist between isolated agent tool calls).

### Session Initialization (Silent Join)
- When starting or joining a session, **never** broadcast an announcement like "session online", "hello", or test messages into the default room.
- Run `iac read -n 20` to silently catch up on recent context and see what other agents are working on.
- Arm your monitor mechanism in the same turn so you receive subsequent incoming messages.

### Monitoring for Messages
Different agent harnesses handle long-running or background processes differently:

1. **Antigravity (and harnesses with command execution / background tasks):**
   - Run `IAC_NAME=<role> iac monitor --once` as a background task via `run_command`.
   - `iac monitor --once` (or `-1`) monitors the room, suppresses messages from `<role>`, and **exits immediately with code 0** upon receiving a single new message from any other agent or user.
   - The harness automatically wakes the agent reactively with `MESSAGE_PRIORITY_HIGH` containing the message text.
   - After processing the message and optionally replying, re-arm `IAC_NAME=<role> iac monitor --once` in the background.

2. **Claude Code (and harnesses with persistent stream tools):**
   - Arm `IAC_NAME=<role> iac monitor` using Claude's persistent `Monitor` tool (`persistent: true`).
   - Messages stream continuously line-by-line.
   - Alternatively, Claude Code can also run `IAC_NAME=<role> iac monitor --once` in a background Bash task.

`IAC_NAME` on the monitor suppresses your own publishes so they never wake you.

### Room Decorum and Attention Conservation
- **Every message wakes every agent monitoring that room.** A broadcast costs attention and tokens for all listening agents and humans. Make every message count.
- The **default room** is the team channel. Use it only for:
  - High-level announcements (e.g. starting or completing significant tasks, breaking builds, modifying shared contracts/APIs).
  - Checking alignment or catching newly joined agents up.
  - Inviting specific agents to a breakout room.
- Always address specific agents with `@<role>`.
- Only respond if a message specifically concerns your role, task, or shared files.
- **Never** announce mere presence or idle status.

### Breakout Rooms (`#<room-name>`)
- Move any back-and-forth dialogue, design debates, paired debugging, or noisy multi-turn interactions to a breakout room.
- Invite the other agent in the default room: e.g. `iac publish "@iac-claude join me in #build-fix" --from iac-antigravity`.
- Both agents switch to the breakout room using `--room '#<name>'` or `IAC_DIR`:
  ```bash
  iac publish "Ready to troubleshoot" --room '#build-fix' --from iac-antigravity
  IAC_NAME=iac-antigravity iac monitor --room '#build-fix' --once
  ```
- When the discussion or task in the breakout concludes, post a single summary message back to the default room if relevant to the rest of the team.

### Commands Reference
```bash
# Read recent history
iac read -n 20
iac read -n 50 --room '#breakout'

# Publish a message
iac publish "build is green" --from <role>
iac publish "pairing on refactor" --room '#breakout' --from <role>

# Monitor for new messages
IAC_NAME=<role> iac monitor --once                # One-shot reactive wait (ideal for Antigravity)
IAC_NAME=<role> iac monitor                       # Continuous stream (ideal for Claude Code Monitor)
IAC_NAME=<role> iac monitor --room '#breakout' --once

# List active rooms
iac rooms
```

Every command takes `--room <[dir | [user@]host:dir][#name]>` (`IAC_DIR` takes the
same forms; a bare `#name` composes with it). Remote stores re-invoke iac over
ssh, so the host needs iac installed and key-based ssh auth. `iac help` prints
the full usage. Source: `~/projects/iac`.

---

## Build and Development

```bash
just build        # Release build into build/ with Ninja
just test         # Build everything + ctest
just run help     # Build + run iac with the given arguments
just install      # Build + install iac into ~/.local/bin
just update-deps  # Move branch-pinned deps to their heads, rewrite lockstep.lock
just format       # clang-format -i on all source files
just clean        # Delete build/
```

Dependencies are managed by lockstep (`tamber-inc/emberstore` → `eacp` + `Miro`), pinned in `lockstep.lock`. Branch-pinned deps (eacp@develop, emberstore@main) stay at the commit in `lockstep.lock` until you run `just update-deps` or `lockstep update <Name>`. To develop against the local checkouts:

```bash
just build -DLOCKSTEP_emberstore_SOURCE=$HOME/projects/emberstore \
           -DLOCKSTEP_eacp_SOURCE=$HOME/projects/eacp
```

## Code Style

- **House Style**: eacp standard (Allman braces, 4-space indentation, 85-column margin, `auto` for locals).
- **Comments**: Keep comments minimal; only comment when code cannot express a critical constraint.
- Always run `clang-format -i` (or `just format`) on modified files before committing (the config is checked in).
