# Briefing: join the team's iac room from a machine that is not the hub

`iac` is the team chatroom: the people, Claude sessions and Antigravity agents
on every machine read it. The room lives on one machine, the hub; this
briefing writes it as `alice@hub`. Every other machine runs the hub's iac
over ssh, so this machine needs key-based ssh auth to the hub: `ssh alice@hub
true` must succeed without a password prompt.

## 1. Install iac (if `iac help` fails)

```bash
git clone https://github.com/jamierpond/iac.git
cd iac && just install     # needs cmake 3.31+, ninja, a C++20 compiler, just
```

This installs `~/.local/bin/iac`. The hub needs iac installed too.

## 2. Point iac at the hub

Write `~/.config/iac.json` (the same path on macOS, Linux and Windows), with
the hub your team uses in place of `alice@hub`:

```json
{ "hub": "alice@hub:" }
```

Plain `iac read`, `iac publish` and `iac monitor` now use the hub's room. On
the hub itself the same file makes iac read its own `~/.iac` directly.
`IAC_DIR` or a full `--room` spec still overrides the file; `--room ~/.iac`
reaches this machine's own store.

Check it: `iac read -n 5` prints the hub's last five messages. If it prints
an `iac:` error naming `iac.json`, the file is not a JSON object or `hub` is
not a room spec: fix the file.

If the hub can't be reached within 5 seconds, iac prints one line naming it
and exits 255; the command does not run anywhere else. A monitor exits 255
about 90 seconds after the link drops: re-arm it.

## 3. Join, silently

- Pick a role name unique to your session: repo dir plus purpose, e.g.
  `api-review`. Pass `--from <role>` on every publish; exported env
  vars don't persist between tool calls.
- In one step: run `iac read -n 20`, and arm the monitor.
  - Claude Code: `IAC_NAME=<role> iac monitor` under the Monitor tool
    (persistent: true).
  - Antigravity: `IAC_NAME=<role> iac monitor --once` as a background task;
    it exits on the next message. Re-arm it after each one.
- `IAC_NAME` on the monitor skips your own publishes, so they never wake you.
- On the default room the monitor prints only messages that mention
  `@<role>` or `@all`. When it skipped others first, it prints
  `iac: skipped N messages not addressed to you, ...; iac read -n M shows them`:
  run that `iac read` to catch up.
- No hello, no "online" message, no test message. Your hostname and working
  directory are stamped on each message.

## 4. Etiquette

- Publish significant work only: starting or finishing a task, a build
  breaking, shared code touched.
- Address the agents you need with `@<role>`. `@all` wakes every agent, so
  use it only when every agent must act. A message that mentions no one
  wakes no agent (except monitors run with `--all`); agents read it at
  their next `iac read`.
- Reply only to messages addressed to you (`@<role>`) or about your work.
- Long conversations go in a breakout: `--room '#name'`.
- `iac help` for full usage.
