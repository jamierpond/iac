#!/bin/bash
# Plugin monitor: runs 'iac monitor' under this session's role for the whole
# session. Restarts it when the role is renamed and when it exits, which it
# does with 255 when the ssh link to the hub drops.

scriptDirectory="$(cd "$(dirname "$0")" && pwd)"
. "$scriptDirectory/session-role-files.sh"

secondsBetweenRoleChecks=2
secondsBeforeRestartingAfterExit=10
room="${IAC_CLAUDE_ROOM:-}"

if [ -z "$CLAUDE_PID" ]; then
    echo "iac plugin: CLAUDE_PID is not set, so the monitor cannot find this session's role. Arm IAC_NAME=<role> iac monitor yourself."
    exit 1
fi

roleFile="$(getRoleFileForClaudeProcess "$CLAUDE_PID")"
roomArguments=()
if [ -n "$room" ]; then
    roomArguments=(--room "$room")
fi

iacMonitorProcessId=""

stopIacMonitor()
{
    if [ -n "$iacMonitorProcessId" ]; then
        kill "$iacMonitorProcessId" 2>/dev/null
        wait "$iacMonitorProcessId" 2>/dev/null
        iacMonitorProcessId=""
    fi
}
trap 'stopIacMonitor; exit 0' INT TERM HUP
trap 'stopIacMonitor' EXIT

isIacMonitorRunning()
{
    kill -0 "$iacMonitorProcessId" 2>/dev/null
}

watchedRole=""
while true; do
    currentRole="$(readRoleFromFile "$roleFile")"
    if [ -z "$currentRole" ]; then
        sleep "$secondsBetweenRoleChecks"
        continue
    fi

    if [ "$currentRole" != "$watchedRole" ]; then
        stopIacMonitor
        watchedRole="$currentRole"
    fi

    if [ -z "$iacMonitorProcessId" ]; then
        IAC_NAME="$watchedRole" iac monitor "${roomArguments[@]}" &
        iacMonitorProcessId=$!
    elif ! isIacMonitorRunning; then
        wait "$iacMonitorProcessId"
        echo "iac plugin: 'iac monitor' exited with status $?; restarting in $secondsBeforeRestartingAfterExit seconds" >&2
        iacMonitorProcessId=""
        sleep "$secondsBeforeRestartingAfterExit"
        continue
    fi

    sleep "$secondsBetweenRoleChecks"
done
