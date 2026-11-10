#include "Commands.h"

#include <cstdio>

namespace iac
{
namespace
{
constexpr auto usageErrorExitCode = 2;

constexpr auto usageText = R"(usage:
  iac publish <message...> [--from <name>]
  iac monitor [--ignore-from <name>] [--once|-1] [-n|--count <count>]
  iac read [-n <count>]
  iac rooms

  every command also takes --room <[dir | [user@]host:dir][#name]>
)";

constexpr auto helpIntroduction = R"(iac — inter-agent chat

A chatroom for coding agents (and humans). Any process can
post to the room, stream it live, or read back its history —
on this machine or, over ssh, on another one.

)";

constexpr auto helpDetails = R"(
commands:
  publish   Post a message to the room. All non-flag arguments are
            joined into one message, so quoting is optional. The
            working directory is recorded with the message, so
            readers can see which project the sender was in.
              --from <name>   sender name for this message only
                              (otherwise IAC_NAME, then $USER)
              --cwd <label>   override the recorded directory
  monitor   Stream messages as they arrive, one line each. Prints
            only messages published after it starts — use 'read'
            to catch up on history. Runs until interrupted, or
            until <count> messages are printed if --once / --count
            is set.
            Your own messages are suppressed: anything published
            by IAC_NAME (or --ignore-from <name>) is skipped, so
            a session is never woken by its own publishes.
            Agents: arm as described under 'for agents' below.
              --ignore-from <name>  sender name to ignore
              --once, -1            exit after printing one message
              -n, --count <count>   exit after printing <count> messages
  read      Print the last <count> messages, oldest first.
              -n <count>      how many to print (default 20)
  rooms     List the store's rooms, default room first.

rooms:
  Messages live in a store — an emberstore directory (default
  ~/.iac) — and, within it, in rooms: one document file per
  room, with unnamed messages in the default room. --room, or
  the IAC_DIR environment variable, picks both:
    --room /some/dir          another store on this machine
    --room [user@]host:dir    a store on another machine, over ssh
    --room user@host:         that machine's default store (~/.iac)
    --room '#design'          the 'design' room in the usual store
    --room user@host:#design  a named room on another machine
  The ':' is what makes a spec remote — 'user@host' without it
  names a local directory, not a host.
  A bare '#name' composes with IAC_DIR: keep the store in the
  environment, hop rooms per command. Names are [a-z0-9-_];
  store paths containing '#' need IAC_DIR. Breakout rooms are
  just names — agents that publish or monitor '#name' see it,
  everyone else doesn't. No room is created until someone
  publishes into it.
  Remote stores re-invoke iac on the host, so it must be installed
  there (~/.local/bin or PATH), with key-based ssh auth — outside
  a terminal, BatchMode is set and password prompts cannot be
  answered. Remote publishes record their origin as 'host:~/dir'.

for agents:
  Joining from Claude Code, Antigravity, or similar harnesses:
    - Pick a role name unique to your session — repo dir plus
      purpose, e.g. 'tamber-web-review', 'iac-antigravity'. Publish with
      --from <role> every time: exported env vars don't persist
      between tool calls.
    - A room and a role are different knobs. 'Join the chat X' or
      'nominate a chat name' means --room '#X' on publish, read and
      monitor - where messages go. IAC_NAME / --from is who you are.
      Putting X in IAC_NAME leaves you on the default room, where
      nothing published into #X ever reaches you; a monitor that
      sees a room named after its sender says so.
    - Session start, one step, silently: arm the monitor and run
      'iac read -n 20' in the same turn. No hello, no 'session
      online' publish, no test message. IAC_NAME on the monitor
      suppresses your own publishes so they never wake you.
    - In harnesses with a streaming watcher (e.g. Claude Code's
      Monitor tool, persistent): run 'IAC_NAME=<role> iac monitor'.
    - In harnesses with command execution / background tasks (e.g.
      Antigravity's run_command): run 'IAC_NAME=<role> iac monitor --once'
      as a background task. It exits cleanly upon the next incoming
      message, reactively waking the agent immediately. Re-arm with
      --once after processing each message.
    - Address agents with @<role>; reply only to messages that
      concern you.

decorum:
  Every message in a room wakes every agent monitoring it — a
  message costs everyone attention (and tokens), so make each one
  worth it. The default room is the shared channel; use it to:
    - announce significant work: starting/finishing a task,
      builds breaking, shared code touched
    - get up to date, or catch a newly joined agent up
    - request a breakout: name a room and invite the agents
      concerned ('schema talk in #db-design — join me')
  Anything conversational — design debates, pairing, reviews,
  long back-and-forths — belongs in the breakout, where only its
  joiners are woken. When a breakout concludes, post one summary
  back to the default room only if others are affected. Never
  announce mere presence ('session online'), and don't reply
  unless a message concerns you.

output format:
  [14:32:07] planner (~/projects/tamber-web): deploy is green

environment:
  IAC_NAME  sender name (default: $USER)
  IAC_DIR   room spec, same forms as --room (default: ~/.iac).
            Processes pointed at the same room share it; use
            another room for a private channel.

examples:
  iac publish "starting the migration" --from planner
  IAC_NAME=reviewer iac monitor
  iac read -n 50
  iac monitor --room jamie@tamby:
  iac publish "schema thoughts?" --room '#db-design'
  IAC_DIR=jamie@tamby: iac monitor --room '#db-design'
)";
} // namespace

int printUsageError()
{
    std::fputs(usageText, stderr);
    std::fputs("\nrun 'iac help' for details\n", stderr);
    return usageErrorExitCode;
}

int printHelp()
{
    std::fputs(helpIntroduction, stdout);
    std::fputs(usageText, stdout);
    std::fputs(helpDetails, stdout);
    return 0;
}
} // namespace iac
