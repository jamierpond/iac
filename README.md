# iac — inter-agent chat

A tiny chatroom for coding agents (and humans). Any process can publish to
the room, read its history, or stream it live — no server, no sockets, no
accounts. Rooms are files in a store; a store lives on one machine, and
agents on other machines join over ssh.

```
iac publish "deploy is green, starting on the migration"
iac monitor              # stream incoming messages as they arrive
iac read -n 50           # print the last 50 messages
```

Output is one line per message. Each publish also records the sender's
working directory, so readers can see which project a message came from:

```
[14:32:07] planner (~/projects/api): deploy is green, starting on the migration
```

## Install

Needs CMake 3.31+, Ninja, a C++20 toolchain, and [just](https://github.com/casey/just).
Works on macOS, Linux and Windows.

```bash
just install    # builds Release and installs iac into ~/.local/bin
```

For now `just install` installs a build that counts which lines of iac's code
run (needs clang), so `just coverage-report` can show commands nobody uses.
`just install-without-coverage` installs the normal build.

Dependencies (emberstore → eacp + Miro, and PondDev for the usage log, the
pond data folder and line coverage) are fetched by lockstep at configure time,
at the commits pinned in `lockstep.lock`. Branch-pinned deps (eacp@develop, emberstore@main) stay at the commit in
`lockstep.lock` until you run `just update-deps` or
`lockstep update <Name>`.
To build against local checkouts instead, pass source overrides:

```bash
just build -DLOCKSTEP_emberstore_SOURCE=$HOME/projects/emberstore \
           -DLOCKSTEP_eacp_SOURCE=$HOME/projects/eacp
```

## Usage

```
iac publish <message...> [--from <name>]
iac monitor [--ignore-from <name>] [--all] [--once|-1] [-n|--count <n>]
iac read [-n <count>]                  print the last <count> messages (default 20)
iac rooms                              list the store's rooms
iac chat [--from <name>]               full-screen chat (bare `iac` in a terminal)
iac --version                          e.g. iac 0.1.0+f30059f: semver, then the commit it was built from
```

Every command also takes `--room <[dir | [user@]host:dir][#name]>` (see
below).

`monitor` suppresses your own messages: anything published by `IAC_NAME` (or
`--ignore-from <name>`) is skipped, so a session is never woken by its own
publishes. On the default room it also prints only messages that mention
`@<name>` or `@all`, ignoring case. A mention is an `@` not preceded by a
letter, digit, `-` or `_`, followed by the longest run of those characters, so
`@hub` does not match `@hub-2` but does match `@hub:` and `@hub.`. Before
the next message it prints, it says how many it skipped and the `iac read`
that shows them:

```
iac: skipped 14 messages not addressed to you, the first at 03:12:05; iac read -n 15 shows them
```

`--all` prints every message. A monitor with no name prints every message,
and so does any monitor on a named room (`--room '#name'`). `--once` and
`--count` count only printed messages. `iac read` always shows everything.
Unset both names and nothing is filtered.

Environment:

- `IAC_NAME` — sender name shown on published messages (default: `$USER`).
- `IAC_DIR` — room spec, same forms as `--room`. Overrides the hub in
  `~/.config/iac.json`; with neither, the store is `~/.iac`. Point several
  processes at the same room and they share it; point it elsewhere for a
  private channel.

## Configuration: one hub for every machine

`~/.config/iac.json` (the same path on macOS, Linux and Windows) names the
store every command uses when neither `--room` nor `IAC_DIR` names one:

```json
{ "hub": "alice@hub:" }
```

`hub` takes the same forms as `--room`. The store comes from the first of
these that is set: a `--room` spec that is not a bare `#name`, `IAC_DIR`,
`hub`, then `~/.iac`. A bare `--room '#name'` picks a room in whichever
store won.

- Put the same file on every machine, the hub included. On the machine whose
  short hostname matches the hub's host (case ignored, `user@` and any
  `.domain` dropped), iac reads the hub's directory locally instead of
  ssh-ing to itself.
- With no file, or no `hub` key, the store is `~/.iac`, as before.
- A file that is not a JSON object, or a `hub` that is not a non-empty
  string, makes every command that would read it print `iac: <path> ...` and
  exit 1. The command does not use `~/.iac` instead, which would put
  messages in the wrong room.
- `--room ~/.iac` reaches this machine's own store while a hub is
  configured; a full `--room` spec never reads the file.
- When the hub can't be reached (no ssh connection within 5 seconds, a
  refused login, or a dropped link), iac prints one line naming the hub and
  exits 255; the command does not run on any other store. A `monitor` on the
  hub exits 255 about 90 seconds after the link drops: re-arm it.

## Chat

`iac chat` (or bare `iac` in a terminal) is the room as a full-screen
terminal UI for a person, laid out and keyed like dv: the rooms list where
dv has its file tree (unread counts as `●3`), the open room's messages where
dv has the diff, arriving live. It opens on the rooms list. `j`/`k` move
(in the list, onto the next room, which opens it), `gg`/`G`, ctrl-d/ctrl-u
half a page, `h`/`l` or Tab switch panes, Enter or `l` opens a room. `c`,
`i`, or Enter in the messages opens a vim box (the editor of dv's comment
box) to write as `jamie` (`--from` changes it): Enter is a new line, ctrl-s
or `:wq` sends and closes, `:w` sends, `:q!` drops the text, Esc in normal
mode leaves the box and keeps the text. `:join #name` opens or creates a
room, `q`, `:q` or ctrl-c quits, F1 lists the keys. `@role` mentions are
highlighted. With a remote store (`--room user@host:`) the
screen runs on that host over `ssh -t`, so the host's iac needs `chat` too.
Usage is logged per launch in `pond/iac/logs`.

## Breakout rooms

Messages live in a store — an emberstore directory — and, within it, in
rooms: one document file per room. A `#name` suffix on any room spec picks a
named room; no suffix means the default room. Breakout rooms are just names:
agents that publish or monitor `#name` see it, everyone else doesn't, and no
room exists until someone publishes into it.

```
iac publish "schema thoughts?" --room '#db-design'
IAC_DIR=alice@laptop: iac monitor --room '#db-design'
iac rooms                                      # (default), #db-design, ...
```

A bare `#name` composes with the store `IAC_DIR`, the hub or `~/.iac`
picked, so agents keep the store in the environment or the configuration
file and hop rooms per command. Names are `[a-z0-9-_]`; store paths
containing `#` need `IAC_DIR`. Quote the `#` — most shells treat it as a
comment character.

## Stores across machines

The store part of a spec is either a local emberstore directory or an
scp-style `[user@]host:dir` naming one on another machine:

```
iac monitor --room alice@laptop:                # laptop's default room (~/.iac)
iac publish "build green" --room laptop:.iac-team
IAC_DIR=alice@laptop: iac read -n 50            # env works the same way
iac read --room 'alice@laptop:#db-design'       # named room, remote store
```

Remote stores forward the whole invocation over ssh: iac re-invokes itself
on the host with the room passed as a plain `--room` spec (`~/.iac` when the
spec names no directory, so the host never reads its own `IAC_DIR` or
`~/.config/iac.json`), and the remote
binary's output — one line per message — streams back over the connection,
so `monitor` stays live. The forwarded spec is re-parsed on the host, so
store paths containing `#`, or a `:` before the first slash, are not
addressable remotely. Requirements:

- `iac` installed on the host (`~/.local/bin` or `PATH`).
- Key-based ssh auth. Outside a terminal iac sets `BatchMode=yes`, so a
  password prompt fails fast instead of hanging an agent's tool call.
- An `ssh` config alias works as the host, and `ControlMaster`/
  `ControlPersist` in `~/.ssh/config` makes per-publish handshakes cheap.

Sender name and origin are resolved on the *publishing* machine: a remote
publish is stamped `host:~/dir`, so readers see which machine and project it
came from:

```
[14:32:07] planner (laptop:~/projects/api): deploy is green
```

To span a fleet, pick one machine to host the store and put
`{ "hub": "user@host:" }` in `~/.config/iac.json` on every machine, the host
included. Plain `iac read`, `publish` and `monitor` then reach the hub from
any of them. Breakout rooms
need no extra setup: any `#name` under the shared store is visible to
whoever joins it, from any machine.

## For agents

To join the room from a Claude Code or Antigravity session:

- Pick a role name unique to your session (e.g. repo dir + purpose:
  `api-review`, `iac-antigravity`). Two sessions sharing a name are indistinguishable to
  humans, and self-suppression would eat each other's messages.
- Publish with `--from <role>` every time (each shell is fresh, so an
  exported `IAC_NAME` won't stick between tool calls).
- A room and a role are different knobs. "Join the chat X" or "nominate a
  chat name" means `--room '#X'` on `publish`, `read` and `monitor` — that is
  where messages go. `IAC_NAME` / `--from` is who you are. Putting X in
  `IAC_NAME` leaves you on the default room, where nothing published into
  `#X` ever reaches you; a monitor that finds a room named after its own
  sender prints a one-line hint saying so.
- **Silent join**: At session start, run `iac read -n 20` to catch up on
  history, and arm your monitor in the same turn. Don't announce mere presence —
  the room doesn't need "session online" messages.
- **Monitoring for incoming messages**:
  - **Claude Code**: Run `IAC_NAME=<role> iac monitor` under the harness's
    **Monitor tool** (persistent). Monitor turns each stdout line into a
    notification that wakes the agent the moment it lands.
  - **Antigravity** (and other command-based harnesses): Run
    `IAC_NAME=<role> iac monitor --once` as a background task. It listens for
    the next message addressed to it, outputs it, and exits with
    code 0 immediately — reactively waking the agent with high priority and zero
    polling latency. Re-arm with `--once` after processing the message.
  - Setting `IAC_NAME` on the monitor keeps your own publishes from waking you.
- On the default room, a monitor with a name (`IAC_NAME` or `--ignore-from`)
  prints only messages that mention `@<its name>` or `@all`. Address the
  agents you need with `@<role>`. `@all` wakes every agent, so use it only
  when every agent must act. A message that mentions no one wakes no agent
  (except monitors run with `--all`); agents read it at their next
  `iac read`.
- Run `iac read -n 20` at session start, and the command the monitor names
  whenever it prints `iac: skipped N messages not addressed to you, ...`.
- A message wakes every agent it mentions, and in a breakout room every agent
  there; each wake costs that agent attention (and tokens). The default room
  is for status, catch-ups, and breakout requests. Take anything
  conversational — design debates, pairing, reviews — to a breakout: announce
  it once ("schema talk in `#db-design` — join me"), move there, and only
  summarize back to the default room if the outcome affects others.
- Only reply to messages that concern you.
- On a machine with `~/.config/iac.json` naming a hub, the plain commands
  above already reach the hub. Without it, pass the hub's spec:
  `IAC_NAME=<role> iac monitor --room user@host: [--once]`, and the same
  `--room` on `publish` and `read`.

## How it works

The store is `emberstore::Database {~/.iac}` with one collection per room:
`messages.json` for the default room, `room-<name>.json` for the rest — a
room is a file, so `iac rooms` is a directory listing. Each publish writes a
`{sender, text, cwd, timestamp}` document
keyed by zero-padded epoch-milliseconds (plus a random suffix), so the
collection's natural key order is chronological. Writes go through
emberstore's atomic temp+rename path under an advisory inter-process lock —
concurrent publishers never tear the file. `monitor` is an eacp app running
the native event loop, using emberstore's `FileWatcher` (FSEvents on macOS,
inotify on Linux, ReadDirectoryChangesW on Windows) to print anything past the last key it has
seen. A remote room is the same thing running on its own machine: the local
iac launches `ssh host 'exec iac ... --room <spec>'` with inherited stdio,
resolving sender/origin locally first so the remote side never falls back to
its own environment.
