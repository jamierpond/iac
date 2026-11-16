#!/bin/bash
# SessionStart hook: picks this session's iac role, exports it as IAC_NAME
# for every later Bash command, and shows the model its role and the room.

scriptDirectory="$(cd "$(dirname "$0")" && pwd)"
. "$scriptDirectory/session-role-files.sh"

historyMessageCount=20
sessionIdCharactersInProvisionalRole=4
daysToKeepRoleFiles=30
room="${IAC_CLAUDE_ROOM:-}"

cat > /dev/null

if [ -z "$CLAUDE_PID" ] || [ -z "$CLAUDE_CODE_SESSION_ID" ]; then
    echo "[iac plugin: CLAUDE_PID or CLAUDE_CODE_SESSION_ID is not set, so this session has no iac role. Arm IAC_NAME=<role> iac monitor yourself.]"
    exit 0
fi

chooseRoleForThisSession()
{
    roleFromEarlierRunOfSession="$(readRoleFromFile "$(getRoleFileForSession "$CLAUDE_CODE_SESSION_ID")")"
    roleFromThisProcess="$(readRoleFromFile "$(getRoleFileForClaudeProcess "$CLAUDE_PID")")"
    if [ -n "$roleFromEarlierRunOfSession" ]; then
        echo "$roleFromEarlierRunOfSession"
    elif [ -n "$roleFromThisProcess" ]; then
        echo "$roleFromThisProcess"
    else
        projectDirectoryName="$(basename "${CLAUDE_PROJECT_DIR:-$PWD}")"
        sessionIdPrefix="$(printf '%s' "$CLAUDE_CODE_SESSION_ID" | cut -c1-"$sessionIdCharactersInProvisionalRole")"
        echo "$projectDirectoryName-$sessionIdPrefix"
    fi
}

role="$(chooseRoleForThisSession)"
writeRoleForSession "$role"
find "$roleDirectory" -name '*.role' -mtime +"$daysToKeepRoleFiles" -exec rm -f {} + 2>/dev/null

if [ -n "$CLAUDE_ENV_FILE" ]; then
    roleFileForThisProcess="$(getRoleFileForClaudeProcess "$CLAUDE_PID")"
    printf "export IAC_NAME=\"\$(head -n 1 '%s' 2>/dev/null)\"\n" "$roleFileForThisProcess" >> "$CLAUDE_ENV_FILE"
fi

roomArguments=()
if [ -n "$room" ]; then
    roomArguments=(--room "$room")
fi

echo "[iac: your role this session is '$role'. IAC_NAME is set to it in every Bash command, so 'iac publish' needs no --from. Room messages reach you as notifications from the iac plugin's monitor for the whole session: do not arm 'iac monitor' yourself. If the role is the provisional '<repo dir>-<id>' one, rename it once you know your task, to '<repo dir>-<purpose>': run 'iac-set-role <name>'.]"
echo "[iac, last $historyMessageCount:]"
iac read -n "$historyMessageCount" "${roomArguments[@]}" 2>&1
exit 0
