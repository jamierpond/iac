#!/usr/bin/env bash
# Runs every iac command path that works without the shared chat store or a
# real remote host, and prints stdout, stderr and exit code of each. Diff two
# runs to check a change kept behaviour identical.
#   usage: scripts/smoke.sh [path/to/iac] > transcript.txt
set -u

iac=$(realpath "${1:-build/iac}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

store="$work/store"
mkdir -p "$store" "$work/bin" "$work/cwd"

# A stand-in ssh that prints what it was asked to run, so remote forwarding is
# checked without a host.
cat > "$work/bin/ssh" <<'EOF'
#!/usr/bin/env bash
printf 'ssh-arg: %s\n' "$@"
exit 7
EOF
chmod +x "$work/bin/ssh"

cd "$work/cwd"
unset IAC_DIR
export IAC_NAME=smoke-tester
export PATH="$work/bin:$PATH"

normalise()
{
    sed -e "s#$work#<work>#g" -e 's#^\[[0-9][0-9]:[0-9][0-9]:[0-9][0-9]\]#[time]#'
}

run()
{
    echo "=== $*" | normalise
    "$iac" "$@" > "$work/out" 2> "$work/err" < /dev/null
    echo "exit: $?"
    echo "--- stdout"
    normalise < "$work/out"
    echo "--- stderr"
    normalise < "$work/err"
}

# Starts a monitor with the given arguments, waits until it is watching,
# publishes each "sender:message", then prints what the monitor printed.
monitorWhilePublishing()
{
    local monitorArguments=$1
    shift
    echo "=== monitor $monitorArguments" | normalise
    # shellcheck disable=SC2086
    "$iac" monitor $monitorArguments > "$work/monitor-out" 2> "$work/monitor-err" < /dev/null &
    local monitorProcess=$!
    for _ in $(seq 50); do
        grep -q "monitoring" "$work/monitor-err" 2> /dev/null && break
        sleep 0.1
    done
    for senderAndMessage in "$@"; do
        "$iac" publish "${senderAndMessage#*:}" --from "${senderAndMessage%%:*}" \
            --room "$store"
        sleep 0.2
    done
    wait "$monitorProcess"
    echo "exit: $?"
    echo "--- stdout"
    normalise < "$work/monitor-out"
    echo "--- stderr"
    normalise < "$work/monitor-err"
}

run
run help
run --help
run -h
run frobnicate
run --room 'Bad#Name' rooms
run --room '#UPPER' read
run publish --room "$store"
run publish --from someone --room "$store"
run rooms --room "$store"
run read --room "$store"
run publish hello there --room "$store"
run publish quoted message --from alice --cwd /some/label --room "$store"
run publish into breakout --room "$store#design"
run publish trailing flag --from --room "$store"
run rooms --room "$store"
run read --room "$store"
run read -n 1 --room "$store"
run read -n 0 --room "$store"
run read --room "$store#design"
run read --room "$store#empty"
run rooms --room "$work/missing"
IAC_DIR="$store" run read -n 2
IAC_DIR="$store" run read --room '#design'
IAC_DIR="$store#design" run read

monitorWhilePublishing "--once --room $store" "other:first"
monitorWhilePublishing "-1 --room $store" "other:second"
monitorWhilePublishing "-n 2 --room $store" "other:third" "other:fourth"
monitorWhilePublishing "--count 1 --ignore-from other --room $store" \
    "other:ignored" "alice:unignored"
monitorWhilePublishing "--once --room $store" "smoke-tester:own" "alice:not own"
IAC_NAME= monitorWhilePublishing "--once --room $store" "smoke-tester:no filter"
"$iac" publish "make the breakout" --from x --room "$store#smoke-tester"
monitorWhilePublishing "--once --room $store" "other:after breakout"

run publish remote hello --room user@host:
run publish remote hello --from bob --cwd /label --room 'user@host:~/store#design'
run read --room user@host:/dir
run read -n 3 --room user@host:/dir
run rooms --room user@host:
run monitor --room user@host:
run monitor --once --ignore-from z --room 'user@host:#room'
IAC_NAME= run monitor --room user@host:
IAC_DIR=user@host:/dir run read --room '#design'
run publish not remote --room 'user@host'
