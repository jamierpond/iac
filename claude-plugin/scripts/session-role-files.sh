# Sourced by the plugin's scripts. A session's role lives in two files:
# one keyed by the claude process id, which the monitor and the Bash
# environment read and which survives /clear, and one keyed by session id,
# which lets --resume pick the role back up in a new process.

roleDirectory="${XDG_STATE_HOME:-$HOME/.local/state}/iac/claude-session-roles"

getRoleFileForClaudeProcess()
{
    printf '%s/claude-process-%s.role' "$roleDirectory" "$1"
}

getRoleFileForSession()
{
    printf '%s/session-%s.role' "$roleDirectory" "$1"
}

readRoleFromFile()
{
    if [ -f "$1" ]; then
        head -n 1 "$1"
    fi
}

writeRoleForSession()
{
    mkdir -p "$roleDirectory"
    printf '%s\n' "$1" > "$(getRoleFileForClaudeProcess "$CLAUDE_PID")"
    printf '%s\n' "$1" > "$(getRoleFileForSession "$CLAUDE_CODE_SESSION_ID")"
}
