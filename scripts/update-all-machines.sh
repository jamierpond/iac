#!/usr/bin/env bash
# Puts every machine, and every agent already running on it, on origin/main's iac.
#   1. Installs on each machine in the order given. Name the hub first: every
#      other machine's iac runs the hub's iac over ssh.
#   2. Ends each 'iac monitor' started before its machine's binary was installed.
#      A running monitor keeps the old binary until it exits; when it exits, the
#      agent's harness reports that and the agent arms a new one.
#   usage: scripts/update-all-machines.sh [--dry-run] [--keep-monitors] [--checkout <dir>] <hub> [machine...]
#          a machine is an ssh target, or 'local' for this one
#          --dry-run: installs nothing and ends nothing; prints each machine's
#          installed version and the monitors step 2 would end
#          --keep-monitors: step 1 only, for a change that 'iac monitor' does not use
#          --checkout: the iac checkout on every machine, relative to its home
#          directory or absolute; default: the path of the checkout this script is in
set -u

usage_end_line=14
script_checkout=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
checkout_directory=${script_checkout#"$HOME"/}

is_dry_run=false
is_keeping_monitors=false
while [ $# -gt 0 ]; do
    case $1 in
        --dry-run) is_dry_run=true; shift ;;
        --keep-monitors) is_keeping_monitors=true; shift ;;
        --checkout) checkout_directory=${2:?--checkout needs a directory}; shift 2 ;;
        *) break ;;
    esac
done
if [ $# -eq 0 ]; then
    sed -n "2,${usage_end_line}p" "$0" >&2
    exit 64
fi
quoted_checkout_directory=$(printf %q "$checkout_directory")

ssh_connect_timeout_seconds=10

# The machine's login shell sets PATH; bash then runs the commands from stdin,
# so they behave the same whatever the login shell is. Some login zsh setups
# leave out the directory holding just while login bash adds it: fall back to that.
run_on_machine() {
    local machine=$1
    if [ "$machine" = "local" ]; then
        bash -s
    else
        ssh -o BatchMode=yes -o ConnectTimeout="$ssh_connect_timeout_seconds" "$machine" \
            'exec "$SHELL" -lc "command -v just >/dev/null 2>&1 && exec bash -s || exec bash -ls"'
    fi
}

change_to_checkout_command="cd ~ && cd $quoted_checkout_directory"

install_commands="
set -eu
$change_to_checkout_command
"'
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "uncommitted changes to tracked files in $PWD; not building" >&2
    exit 3
fi
git pull --ff-only --quiet
just install >/dev/null
"$HOME/.local/bin/iac" --version
'

print_installed_version_command='"$HOME/.local/bin/iac" --version'

# Prints "<pid> <command>" for each 'iac monitor' older than the installed binary.
list_stale_commands='
binary="$HOME/.local/bin/iac"
installed_at=$(stat -c %Y "$binary" 2>/dev/null || stat -f %m "$binary")
seconds_since_install=$(( $(date +%s) - installed_at ))
ps axo pid=,etime=,args= | awk -v since_install="$seconds_since_install" "
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
        if (is_monitor && to_seconds(elapsed) > since_install) print pid, \$0
    }"
'

end_stale_monitor_commands="stale=\$( { $list_stale_commands } )
if [ -z \"\$stale\" ]; then echo 'no monitor predates the installed iac'; exit 0; fi
echo \"\$stale\" | cut -c1-160
if [ \"$is_dry_run\" = false ]; then echo \"\$stale\" | awk '{print \$1}' | xargs kill 2>/dev/null; fi
"

exit_code=0
expected_version=""
reached_machines=()

echo "== install, hub first"
for machine in "$@"; do
    if [ "$is_dry_run" = true ]; then
        version=$(echo "$print_installed_version_command" | run_on_machine "$machine" 2>&1)
    else
        version=$(echo "$install_commands" | run_on_machine "$machine" 2>&1)
    fi
    if [ $? -ne 0 ]; then
        echo "$machine: NOT UPDATED: $(echo "$version" | tail -1)"
        exit_code=1
        continue
    fi
    version=$(echo "$version" | tail -1)
    echo "$machine: $version"
    reached_machines+=("$machine")
    if [ -z "$expected_version" ]; then
        expected_version=$version
    elif [ "$version" != "$expected_version" ]; then
        echo "$machine: has '$version', the hub has '$expected_version'"
        exit_code=1
    fi
done

if [ $exit_code -ne 0 ] && [ "$is_dry_run" = false ]; then
    echo "not ending any monitor: fix the machines above first" >&2
    exit $exit_code
fi
if [ "$is_keeping_monitors" = true ]; then
    exit $exit_code
fi

# The hub goes last: ending a monitor there also ends the ssh of the machine
# watching through it, and that machine's own old monitors should already be gone.
echo "== monitors started before the install, hub last"
for (( index = ${#reached_machines[@]} - 1; index >= 0; index-- )); do
    machine=${reached_machines[$index]}
    echo "-- $machine"
    echo "$end_stale_monitor_commands" | run_on_machine "$machine"
done

exit $exit_code
