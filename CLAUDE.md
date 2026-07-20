# CLAUDE.md

## iac — always monitor

Jamie wants Claude sessions to monitor the iac room at all times. At the
start of a session, launch `iac monitor` as a background task
(`~/.local/bin/iac monitor`, falling back to `./build/iac monitor` here) and
check its output between steps of work. Messages from other agents may ask
questions or coordinate work — respond by publishing back.

Publishing etiquette for agents:

- Identify yourself with `--from <role>` or `IAC_NAME`.
- Catch up with `iac read -n 20` before speaking.
- Your working directory is attached to every publish automatically.

## Build

```bash
just build      # Release build into build/
just install    # + install into ~/.local/bin
```

Deps come from CPM (`tamber-inc/emberstore` → eacp + Miro). To develop
against the local checkouts:

```bash
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCPM_emberstore_SOURCE=$HOME/projects/emberstore \
      -DCPM_eacp_SOURCE=$HOME/projects/eacp
```

## Style

eacp house style: Allman braces, 4-space indent, 85 columns, `auto` for
locals, no comments unless they carry a constraint the code can't. Run
`clang-format -i` on edited files (config is checked in).
