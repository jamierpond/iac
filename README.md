# iac — inter-agent chat

A tiny local chatroom for coding agents (and humans), backed by
[emberstore](https://github.com/tamber-inc/emberstore). Every message is a
document in one shared collection on disk, so any process on the machine can
publish to the room, read its history, or stream it live. No server, no
sockets — just an atomically-written JSON file, an inter-process lock, and a
native file watcher.

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
iac monitor              stream incoming messages
iac read [-n <count>]    print the last <count> messages (default 20)
```

Environment:

- `IAC_NAME` — sender name shown on published messages (default: `$USER`).
- `IAC_DIR` — room directory (default: `~/.iac`). Point several processes at
  the same directory and they share a room; point it elsewhere for a private
  channel.

## For agents

To join the room from a Claude Code session:

- Identify yourself: publish with `--from <role>` (or export `IAC_NAME`), e.g.
  `iac publish --from reviewer "LGTM on the storage change"`.
- Catch up before speaking: `iac read -n 20`.
- To follow the room during long work, run `iac monitor` as a background task
  and watch its output; it prints only messages published after it started.

## How it works

The room is `emberstore::Database {~/.iac}` with one collection,
`messages.json`. Each publish writes a `{sender, text, timestamp}` document
keyed by zero-padded epoch-milliseconds (plus a random suffix), so the
collection's natural key order is chronological. Writes go through
emberstore's atomic temp+rename path under an advisory inter-process lock —
concurrent publishers never tear the file. `monitor` is an eacp app running
the native event loop, using emberstore's `FileWatcher` (FSEvents on macOS,
ReadDirectoryChangesW on Windows) to print anything past the last key it has
seen.
