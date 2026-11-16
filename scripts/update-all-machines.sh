#!/usr/bin/env bash
# Puts every machine, and every agent already running on it, on origin/main's iac.
#   1. Installs on each machine in the order given. Name the hub first: every
#      other machine's iac runs the hub's iac over ssh.
#   2. Ends each 'iac monitor' started before its machine's binary was installed.
#      A running monitor keeps the old binary until it exits; when it exits, the
#      agent's harness reports that and the agent arms a new one.
# 'iac chat' screens are listed, never ended: a person is using them.
#   usage: scripts/update-all-machines.sh [--dry-run] <hub> [machine...]
#          a machine is an ssh target, or 'local' for this one
set -u

is_dry_run=false
if [ "${1:-}" = "--dry-run" ]; then
    is_dry_run=true
    shift
fi
if [ $# -eq 0 ]; then
    sed -n '2,11p' "$0" >&2
    exit 64
fi

ssh_connect_timeout_seconds=10

# The machine's login shell sets PATH (nix, ~/.local/bin); bash then runs the
# commands from stdin, so they behave the same whatever the login shell is.
run_on_machine() {
    local machine=$1
    if [ "$machine" = "local" ]; then
        bash -s
    else
        ssh -o BatchMode=yes -o ConnectTimeout="$ssh_connect_timeout_seconds" "$machine" \
            'exec "$SHELL" -lc "exec bash -s"'
    fi
}

install_commands='
set -eu
# just and cmake come from nix on the Linux hub, and its login zsh does not add them.
PATH="$HOME/.nix-profile/bin:$PATH"
cd ~/projects/iac
if [ -n "$(git status --porcelain)" ]; then
    echo "uncommitted changes in ~/projects/iac; not building" >&2
    exit 3
fi
git pull --ff-only --quiet
just install >/dev/null
git rev-parse --short HEAD
'

# Prints "<pid> <command>" for each iac process older than the installed binary.
list_stale_commands='
binary="$HOME/.local/bin/iac"
installed_at=$(stat -c %Y "$binary" 2>/dev/null || stat -f %m "$binary")
seconds_since_install=$(( $(date +%s) - installed_at ))
ps axo pid=,etime=,args= | awk -v since_install="$seconds_since_install" -v wanted="$wanted" "
    function to_seconds(elapsed,    parts, count, days) {
        days = 0
        if (split(elapsed, parts, \"-\") == 2) { days = parts[1]; elapsed = parts[2] }
        count = split(elapsed, parts, \":\")
        if (count == 3) return days * 86400 + parts[1] * 3600 + parts[2] * 60 + parts[3]
        return days * 86400 + parts[1] * 60 + parts[2]
    }
    {
        pid = \$1; elapsed = \$2; \$1 = \"\"; \$2 = \"\"; sub(/^ +/, \"\")
        is_monitor = (\$0 ~ /^([^ ]*\\/)?iac monitor/) || (\$0 ~ /^ssh .*exec iac .monitor./)
        is_chat = (\$0 ~ /^([^ ]*\\/)?iac( chat|\$)/) || (\$0 ~ /^ssh .*exec iac .chat./)
        is_wanted = (wanted == \"monitor\" && is_monitor) || (wanted == \"chat\" && is_chat)
        if (is_wanted && to_seconds(elapsed) > since_install) print pid, \$0
    }"
'

end_stale_monitor_commands="wanted=monitor
stale=\$( { $list_stale_commands } )
if [ -z \"\$stale\" ]; then echo 'no monitor predates the installed iac'; exit 0; fi
echo \"\$stale\" | cut -c1-160
if [ \"$is_dry_run\" = false ]; then echo \"\$stale\" | awk '{print \$1}' | xargs kill 2>/dev/null; fi
"

list_stale_chat_commands="wanted=chat
$list_stale_commands"

exit_code=0
expected_commit=""
reached_machines=()

echo "== install, hub first"
for machine in "$@"; do
    if [ "$is_dry_run" = true ]; then
        commit=$(echo 'cd ~/projects/iac && git rev-parse --short HEAD' | run_on_machine "$machine" 2>&1)
    else
        commit=$(echo "$install_commands" | run_on_machine "$machine" 2>&1)
    fi
    if [ $? -ne 0 ]; then
        echo "$machine: NOT UPDATED: $(echo "$commit" | tail -1)"
        exit_code=1
        continue
    fi
    commit=$(echo "$commit" | tail -1)
    echo "$machine: $commit"
    reached_machines+=("$machine")
    if [ -z "$expected_commit" ]; then
        expected_commit=$commit
    elif [ "$commit" != "$expected_commit" ]; then
        echo "$machine: is on $commit, the hub is on $expected_commit"
        exit_code=1
    fi
done

if [ $exit_code -ne 0 ] && [ "$is_dry_run" = false ]; then
    echo "not ending any monitor: fix the machines above first" >&2
    exit $exit_code
fi

# The hub goes last: ending a monitor there also ends the ssh of the machine
# watching through it, and that machine's own old monitors should already be gone.
echo "== monitors started before the install, hub last"
for (( index = ${#reached_machines[@]} - 1; index >= 0; index-- )); do
    machine=${reached_machines[$index]}
    echo "-- $machine"
    echo "$end_stale_monitor_commands" | run_on_machine "$machine"
    stale_chats=$(echo "$list_stale_chat_commands" | run_on_machine "$machine")
    if [ -n "$stale_chats" ]; then
        echo "left running, still on the old iac (quit and reopen): "
        echo "$stale_chats" | cut -c1-160
    fi
done

exit $exit_code
