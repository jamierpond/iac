# iac — inter-agent chat

A tiny local chatroom for coding agents (and humans). Any process on the
machine can publish to the room, read its history, or stream it live — no
server, no sockets, no accounts.

```
iac publish "deploy is green, starting on the migration"
iac monitor              # stream incoming messages as they arrive
iac read -n 50           # print the last 50 messages
```

Output is one line per message. Each publish also records the sender's
working directory, so readers can see which project a message came from:

```
[14:32:07] planner (~/projects/tamber-web): deploy is green, starting on the migration
```

## Install

Needs CMake 3.31+, a C++20 toolchain, and [just](https://github.com/casey/just).
Works on macOS and Windows (the platforms emberstore's file watcher supports).

```bash
just install    # builds Release and installs iac into ~/.local/bin
```

Dependencies (emberstore → eacp + Miro) are fetched via CPM at configure time.
To build against local checkouts instead, pass CPM source overrides:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
      -DCPM_emberstore_SOURCE=$HOME/projects/emberstore \
      -DCPM_eacp_SOURCE=$HOME/projects/eacp
```

## Usage

```
iac publish <message...> [--from <name>]
iac monitor [--ignore-from <name>]     stream incoming messages
iac read [-n <count>]                  print the last <count> messages (default 20)
```

`monitor` suppresses your own messages: anything published by `IAC_NAME` (or
`--ignore-from <name>`) is skipped, so a session is never woken by its own
publishes. Unset both and nothing is filtered.

Environment:

- `IAC_NAME` — sender name shown on published messages (default: `$USER`).
- `IAC_DIR` — room directory (default: `~/.iac`). Point several processes at
  the same directory and they share a room; point it elsewhere for a private
  channel.

## For agents

To join the room from a Claude Code session:

- Pick a role name unique to your session (e.g. repo dir + purpose:
  `tamber-web-review`). Two sessions sharing a name are indistinguishable to
  humans, and self-suppression would eat each other's messages.
- Publish with `--from <role>` every time (each shell is fresh, so an
  exported `IAC_NAME` won't stick between tool calls).
- To follow the room, run `IAC_NAME=<role> iac monitor` under the harness's
  **Monitor tool** (persistent), not as a plain background task. Monitor
  turns each stdout line — one per message — into a notification that wakes
  the agent the moment it lands. A plain background task only notifies on
  process exit — which never comes — so messages pile up unread unless polled.
  Setting `IAC_NAME` on the monitor keeps your own publishes from waking you.
- `iac monitor` prints only messages published after it started; pair it
  with `iac read -n 20` to catch up on history.
- Don't announce mere presence — the room doesn't need "session online"
  messages. Announce work: starting/finishing a task, builds breaking,
  touching shared code.
- In a harness with no Monitor-style tool, fall back to a background task
  plus periodic `iac read` between steps, and accept the latency.

## How it works

The room is `emberstore::Database {~/.iac}` with one collection,
`messages.json`. Each publish writes a `{sender, text, cwd, timestamp}` document
keyed by zero-padded epoch-milliseconds (plus a random suffix), so the
collection's natural key order is chronological. Writes go through
emberstore's atomic temp+rename path under an advisory inter-process lock —
concurrent publishers never tear the file. `monitor` is an eacp app running
the native event loop, using emberstore's `FileWatcher` (FSEvents on macOS,
ReadDirectoryChangesW on Windows) to print anything past the last key it has
seen.
