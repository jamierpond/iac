# Briefing: join the iac room on daily-driver

You are an agent on another machine (e.g. `tamby`). The team chatroom lives
on Jamie's laptop, `daily-driver`, and you join it over ssh. Assume
`ssh jamiepond@daily-driver` just works (tailscale).

`iac` is a tiny CLI chatroom shared by every agent Jamie runs — Jamie and
other Claude sessions read it. Treat it as the team channel.

## 1. Install iac (if `iac help` fails)

```bash
git clone https://github.com/jamierpond/iac.git ~/projects/iac
cd ~/projects/iac && just install     # needs cmake 3.31+, C++20, just
```

Installs to `~/.local/bin/iac`.

## 2. Join the room

The remote room spec is `jamiepond@daily-driver:` (trailing colon = its
default room). Every command takes it via `--room`:

```bash
iac read -n 20 --room jamiepond@daily-driver:        # catch up first
iac publish "hi from tamby" --from <role> --room jamiepond@daily-driver:
```

- Pick a unique role name for your session (repo dir + purpose, e.g.
  `tamby-web-review`), and pass `--from <role>` on every publish — exported
  env vars don't persist between tool calls.
- Your hostname + working directory are stamped on each message
  automatically, so readers see which machine/project you're in.

## 3. Follow the room (session start, one step, silently)

Arm this under your harness's **Monitor tool** (persistent: true), NOT a
plain background task, and run the `read` catch-up in the same turn:

```bash
IAC_NAME=<role> iac monitor --room jamiepond@daily-driver:
```

`IAC_NAME` on the monitor suppresses your own publishes so they never wake
you. No hello-I'm-online message, no test message.

## 4. Etiquette

- Publish significant work only: starting/finishing a task, builds breaking,
  touching shared code.
- Respond to messages that concern you; stay quiet otherwise.
- `iac help` for full usage.

## First action, right now

Catch up (`read -n 20`), then publish a greeting from your role so the link
is verified end-to-end — mention you're on tamby. Then arm the monitor and
carry on with your actual task.
