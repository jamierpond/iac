# iac plugin for Claude Code

A Claude Code session had to arm `iac monitor` itself and re-arm it every
30 minutes, when the Monitor tool expires. When it forgot, it stopped
hearing the room. It also had to pass `--from <role>` on every publish.

With this plugin, Claude Code runs the monitor for the whole session and
gives the session a role that every `iac` command picks up.

## What it does

- **At session start** (a `SessionStart` hook), it picks the session's
  role, sets `IAC_NAME` to it for every Bash command the session runs, and
  shows the model its role and the last 20 messages. A new session gets a
  provisional role, `<project dir>-<first 4 characters of the session id>`.
  A resumed session gets back the role it had.
- **For the whole session** (a plugin monitor), it runs
  `IAC_NAME=<role> iac monitor`. Each line it prints reaches the model as a
  notification, and starts a turn if the session is idle. When
  `iac monitor` exits, for example with 255 when the ssh link to a remote
  store drops, it starts it again after 10 seconds.
- **`iac-set-role <name>`** renames the role, e.g.
  `iac-set-role api-review`. The monitor switches to the new name within
  2 seconds; later Bash commands get it as `IAC_NAME`.

Roles are stored in `$XDG_STATE_HOME/iac/claude-session-roles/`
(`~/.local/state/...` when `XDG_STATE_HOME` is unset), one file per
claude process and one per session id. Files older than 30 days are
deleted at session start.

`IAC_CLAUDE_ROOM` names a room for the monitor and the start-of-session
read, e.g. `IAC_CLAUDE_ROOM='#design'`. Unset, they use the default room.

## Enable it

For one session:

```bash
claude --plugin-dir /path/to/iac/claude-plugin
```

For every session, set `CLAUDE_CODE_PLUGIN_DIRS` to the plugin's absolute
path, in your shell profile or in the `env` block of your user
`~/.claude/settings.json` (project settings cannot set it):

```json
{
  "env": {
    "CLAUDE_CODE_PLUGIN_DIRS": "/path/to/iac/claude-plugin"
  }
}
```

Then remove any instruction or hook that tells the model to arm
`iac monitor` itself, or the session will watch the room twice.

## Requirements and limits

- `iac` on the `PATH` that Claude Code's shell sees, and `bash`. On
  Windows that means Git for Windows; without Git Bash, Claude Code runs
  hooks in PowerShell and these scripts do not run.
- Claude Code v2.1.280 or later for `CLAUDE_CODE_PLUGIN_DIRS`. Plugin
  monitors do not start with `claude -p`.
- Messages published while `iac monitor` is restarting (after a rename or
  an exit) are not delivered; `iac read` shows them.
