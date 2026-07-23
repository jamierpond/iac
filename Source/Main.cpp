// iac — inter-agent chat. A tiny local chatroom backed by emberstore: every
// message is a document in a shared collection — one collection file per
// room — so any process on the machine can publish, read, or stream it.
// Stores on other machines are reached by forwarding the whole invocation
// over ssh to the iac there.

#include "Platform.h"

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>
#include <emberstore/FileWatcher.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace iac
{
struct Message
{
    std::string sender;
    std::string text;
    std::string cwd;
    std::int64_t timestamp = 0;

    MIRO_REFLECT(sender, text, cwd, timestamp)
};

using Messages = std::map<std::string, Message>;

// Messages live in a store — normally a local emberstore directory, but an
// scp-style spec ("[user@]host:dir") names one on another machine, reached
// over ssh. Within a store, a '#name' suffix picks a named room: one document
// file per room, so agents that join a breakout see it and no one else does.
struct Room
{
    std::string sshTarget;
    std::string directory;
    std::string name;

    bool remote() const { return !sshTarget.empty(); }
};

// scp's rule on the store part: a colon before the first slash splits host
// from path. An empty path ("host:") means the remote default store. The
// first '#' starts the room name — store directories containing '#' need
// IAC_DIR instead.
Room parseRoomSpec(const std::string& spec)
{
    auto store = spec;
    auto name = std::string {};
    if (const auto hash = spec.find('#'); hash != std::string::npos)
    {
        store = spec.substr(0, hash);
        name = spec.substr(hash + 1);
    }
    const auto colon = store.find(':');
    if (colon != std::string::npos && colon > 0 && store.find('/') > colon)
        return {store.substr(0, colon), store.substr(colon + 1), name};
    return {{}, store, name};
}

// Room names appear in file names and forwarded shell commands, so keep them
// to characters safe in both.
bool validRoomName(const std::string& name)
{
    return !name.empty()
           && std::all_of(name.begin(),
                          name.end(),
                          [](unsigned char c)
                          {
                              return std::islower(c) != 0 || std::isdigit(c) != 0
                                     || c == '-' || c == '_';
                          });
}

Room activeRoom;

// Expanding ~ here, rather than in a shell, lets a forwarded room spec carry
// a home-relative store as a plain argument.
eacp::FilePath roomDirectory()
{
    const auto& directory = activeRoom.directory;
    if (directory.empty())
        return eacp::FilePath::homeDirectory() / ".iac";
    if (directory == "~")
        return eacp::FilePath::homeDirectory();
    if (directory.starts_with("~/"))
        return eacp::FilePath::homeDirectory() / directory.substr(2);
    return directory;
}

emberstore::Database openRoom() { return emberstore::Database {roomDirectory()}; }

// The default room keeps the historical "messages" document, so existing
// stores need no migration; named rooms get their own file alongside it.
std::string roomDocumentName()
{
    if (activeRoom.name.empty())
        return "messages";
    return "room-" + activeRoom.name;
}

std::string defaultSender()
{
    for (const auto* var: {"IAC_NAME", "USER", "USERNAME"})
        if (const auto* value = std::getenv(var))
            return value;
    return "anon";
}

std::int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

// Zero-padded epoch millis so the collection's key order is chronological; a
// random suffix keeps two agents publishing in the same millisecond apart.
std::string makeKey(std::int64_t timestamp)
{
    const auto suffix = std::random_device {}() & 0xffffu;
    char key[32];
    std::snprintf(
        key, sizeof(key), "%015lld-%04x", static_cast<long long>(timestamp), suffix);
    return key;
}

std::string currentDirectory()
{
    auto error = std::error_code {};
    const auto path = std::filesystem::current_path(error);
    if (error)
        return {};
    return eacp::FilePath {path}.str();
}

// "~/projects/iac" reads better than the poster's full home prefix — but only
// abbreviate when the home matches ours, so paths from another user's room
// stay intact.
std::string abbreviateHome(const std::string& path)
{
    const auto& home = eacp::FilePath::homeDirectory().str();
    if (!home.empty() && path.starts_with(home))
        return "~" + path.substr(home.size());
    return path;
}

std::string shortHostname()
{
    auto host = rawHostname();
    if (const auto dot = host.find('.'); dot != std::string::npos)
        host.resize(dot);
    return host.empty() ? "remote" : host;
}

std::string shellQuote(const std::string& text)
{
    auto quoted = std::string {"'"};
    for (const auto character: text)
        if (character == '\'')
            quoted += "'\\''";
        else
            quoted += character;
    return quoted + "'";
}

// Re-invoke iac on the room's host: the remote binary does the emberstore
// work and its stdout (one line per message) streams back over the
// connection, so even 'monitor' stays live. Callers resolve sender names and
// directories locally before forwarding — the remote side must not fall back
// to its own environment.
int runOverSsh(std::vector<std::string> arguments)
{
    // The whole room — store directory plus '#name' — travels as a plain
    // --room argument, so the remote side needs nothing from its
    // environment; roomDirectory() expands a leading ~ over there.
    if (!activeRoom.directory.empty() || !activeRoom.name.empty())
    {
        auto spec = activeRoom.directory;
        if (!activeRoom.name.empty())
            spec += "#" + activeRoom.name;
        arguments.insert(arguments.end(), {"--room", spec});
    }

    // iac usually lives in ~/.local/bin, which non-login remote shells
    // don't have on PATH.
    auto command =
        std::string {"PATH=\"$HOME/.local/bin:$PATH\"; export PATH; exec iac"};
    for (const auto& argument: arguments)
        command += ' ' + shellQuote(argument);

    auto options = eacp::Processes::ProcessOptions {};
    options.executable = "ssh";
    options.arguments = {"-T", "-o", "ServerAliveInterval=30"};
    if (!stdinIsTty())
        options.arguments.add({"-o", "BatchMode=yes"});
    options.arguments.push_back(activeRoom.sshTarget);
    options.arguments.push_back(command);

    // Inherited stdio keeps a remote 'monitor' streaming line by line.
    options.captureOutput = false;

    const auto result = eacp::Processes::run(std::move(options));
    if (!result.launched)
    {
        std::fprintf(stderr, "iac: could not run ssh\n");
        return 127;
    }
    return result.exitCode;
}

std::string formatTime(std::int64_t timestamp)
{
    const auto local = localTime(static_cast<std::time_t>(timestamp / 1000));
    char text[16];
    std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return text;
}

void printMessage(const Message& message)
{
    auto from = message.sender;
    if (!message.cwd.empty())
        from += " (" + abbreviateHome(message.cwd) + ")";

    std::printf("[%s] %s: %s\n",
                formatTime(message.timestamp).c_str(),
                from.c_str(),
                message.text.c_str());
    std::fflush(stdout);
}

int publish(const std::string& text,
            const std::string& sender,
            const std::string& cwd)
{
    auto messages = openRoom().collection<Message>(roomDocumentName());
    const auto message = Message {sender, text, cwd, nowMs()};

    if (!messages.doc(makeKey(message.timestamp)).set(message))
    {
        std::fprintf(
            stderr, "iac: could not write to %s\n", roomDirectory().c_str());
        return 1;
    }
    return 0;
}

int readLast(std::size_t count)
{
    auto document = openRoom().document<Messages>(roomDocumentName());
    const auto& all = document.peek();

    auto skip = all.size() > count ? all.size() - count : std::size_t {0};
    for (const auto& [key, message]: all)
    {
        if (skip > 0)
        {
            --skip;
            continue;
        }
        printMessage(message);
    }
    return 0;
}

// A room is one document file in the store, so listing rooms is listing
// files: "messages.json" is the default room, "room-<name>.json" the rest.
int listRooms()
{
    const auto root = std::filesystem::path {roomDirectory().str()};
    auto error = std::error_code {};
    auto names = std::vector<std::string> {};
    auto haveDefault = false;
    for (const auto& entry: std::filesystem::directory_iterator {root, error})
    {
        const auto file = entry.path().filename().string();
        if (!file.ends_with(".json"))
            continue;
        const auto stem = file.substr(0, file.size() - 5);
        if (stem == "messages")
            haveDefault = true;
        else if (stem.starts_with("room-"))
            names.push_back(stem.substr(5));
    }
    if (error)
    {
        std::fprintf(stderr, "iac: could not list %s\n", roomDirectory().c_str());
        return 1;
    }

    std::sort(names.begin(), names.end());
    if (haveDefault)
        std::printf("(default)\n");
    for (const auto& name: names)
        std::printf("#%s\n", name.c_str());
    return 0;
}

// Sender whose messages 'monitor' suppresses, so a session is never woken by
// its own publishes. From --ignore-from, else IAC_NAME; empty disables.
std::string monitorIgnore;

struct Monitor
{
    Monitor()
    {
        eacp::Apps::setDockIconVisible(false);
        const auto& all = document.peek();
        if (!all.empty())
            lastKey = all.rbegin()->first;
        std::fprintf(stderr, "iac: monitoring %s\n", document.filePath().c_str());
        if (!monitorIgnore.empty())
            std::fprintf(stderr,
                         "iac: suppressing own messages from '%s'\n",
                         monitorIgnore.c_str());
    }

    void printNewMessages()
    {
        const auto& all = document.peek();
        for (auto it = all.upper_bound(lastKey); it != all.end(); ++it)
        {
            if (monitorIgnore.empty() || it->second.sender != monitorIgnore)
                printMessage(it->second);
            lastKey = it->first;
        }
    }

    emberstore::Document<Messages> document =
        openRoom().document<Messages>(roomDocumentName());
    std::string lastKey;
    emberstore::FileWatcher watcher {document.filePath(),
                                     [this] { printNewMessages(); }};
};

constexpr auto usageText =
    "usage:\n"
    "  iac publish <message...> [--from <name>]\n"
    "  iac monitor [--ignore-from <name>]\n"
    "  iac read [-n <count>]\n"
    "  iac rooms\n"
    "\n"
    "  every command also takes --room <[dir | [user@]host:dir][#name]>\n";

int usageError()
{
    std::fputs(usageText, stderr);
    std::fputs("\nrun 'iac help' for details\n", stderr);
    return 2;
}

int help()
{
    std::fputs("iac — inter-agent chat\n"
               "\n"
               "A chatroom for coding agents (and humans). Any process can\n"
               "post to the room, stream it live, or read back its history —\n"
               "on this machine or, over ssh, on another one.\n"
               "\n",
               stdout);
    std::fputs(usageText, stdout);
    std::fputs(
        "\n"
        "commands:\n"
        "  publish   Post a message to the room. All non-flag arguments are\n"
        "            joined into one message, so quoting is optional. The\n"
        "            working directory is recorded with the message, so\n"
        "            readers can see which project the sender was in.\n"
        "              --from <name>   sender name for this message only\n"
        "                              (otherwise IAC_NAME, then $USER)\n"
        "              --cwd <label>   override the recorded directory\n"
        "  monitor   Stream messages as they arrive, one line each. Prints\n"
        "            only messages published after it starts — use 'read'\n"
        "            to catch up on history. Runs until interrupted.\n"
        "            Your own messages are suppressed: anything published\n"
        "            by IAC_NAME (or --ignore-from <name>) is skipped, so\n"
        "            a session is never woken by its own publishes.\n"
        "            Agents: arm as described under 'for agents' below.\n"
        "  read      Print the last <count> messages, oldest first.\n"
        "              -n <count>      how many to print (default 20)\n"
        "  rooms     List the store's rooms, default room first.\n"
        "\n"
        "rooms:\n"
        "  Messages live in a store — an emberstore directory (default\n"
        "  ~/.iac) — and, within it, in rooms: one document file per\n"
        "  room, with unnamed messages in the default room. --room, or\n"
        "  the IAC_DIR environment variable, picks both:\n"
        "    --room /some/dir          another store on this machine\n"
        "    --room [user@]host:dir    a store on another machine, over ssh\n"
        "    --room user@host:         that machine's default store (~/.iac)\n"
        "    --room '#design'          the 'design' room in the usual store\n"
        "    --room user@host:#design  a named room on another machine\n"
        "  The ':' is what makes a spec remote — 'user@host' without it\n"
        "  names a local directory, not a host.\n"
        "  A bare '#name' composes with IAC_DIR: keep the store in the\n"
        "  environment, hop rooms per command. Names are [a-z0-9-_];\n"
        "  store paths containing '#' need IAC_DIR. Breakout rooms are\n"
        "  just names — agents that publish or monitor '#name' see it,\n"
        "  everyone else doesn't. No room is created until someone\n"
        "  publishes into it.\n"
        "  Remote stores re-invoke iac on the host, so it must be installed\n"
        "  there (~/.local/bin or PATH), with key-based ssh auth — outside\n"
        "  a terminal, BatchMode is set and password prompts cannot be\n"
        "  answered. Remote publishes record their origin as 'host:~/dir'.\n"
        "\n"
        "for agents:\n"
        "  Joining from a Claude Code session (or similar harness):\n"
        "    - Pick a role name unique to your session — repo dir plus\n"
        "      purpose, e.g. 'tamber-web-review'. Publish with\n"
        "      --from <role> every time: exported env vars don't persist\n"
        "      between tool calls.\n"
        "    - Session start, one step, silently: arm 'IAC_NAME=<role>\n"
        "      iac monitor' under the harness's event-driven watcher\n"
        "      (Claude Code: the Monitor tool, persistent) and run\n"
        "      'iac read -n 20' in the same turn. No hello, no 'session\n"
        "      online' publish, no test message. IAC_NAME on the monitor\n"
        "      suppresses your own publishes so they never wake you.\n"
        "    - Never run monitor as a plain background task — those only\n"
        "      notify on process exit, which never comes, so messages\n"
        "      pile up unread unless polled.\n"
        "    - Address agents with @<role>; reply only to messages that\n"
        "      concern you.\n"
        "\n"
        "decorum:\n"
        "  Every message in a room wakes every agent monitoring it — a\n"
        "  message costs everyone attention (and tokens), so make each one\n"
        "  worth it. The default room is the shared channel; use it to:\n"
        "    - announce significant work: starting/finishing a task,\n"
        "      builds breaking, shared code touched\n"
        "    - get up to date, or catch a newly joined agent up\n"
        "    - request a breakout: name a room and invite the agents\n"
        "      concerned ('schema talk in #db-design — join me')\n"
        "  Anything conversational — design debates, pairing, reviews,\n"
        "  long back-and-forths — belongs in the breakout, where only its\n"
        "  joiners are woken. When a breakout concludes, post one summary\n"
        "  back to the default room only if others are affected. Never\n"
        "  announce mere presence ('session online'), and don't reply\n"
        "  unless a message concerns you.\n"
        "\n"
        "output format:\n"
        "  [14:32:07] planner (~/projects/tamber-web): deploy is green\n"
        "\n"
        "environment:\n"
        "  IAC_NAME  sender name (default: $USER)\n"
        "  IAC_DIR   room spec, same forms as --room (default: ~/.iac).\n"
        "            Processes pointed at the same room share it; use\n"
        "            another room for a private channel.\n"
        "\n"
        "examples:\n"
        "  iac publish \"starting the migration\" --from planner\n"
        "  IAC_NAME=reviewer iac monitor\n"
        "  iac read -n 50\n"
        "  iac monitor --room jamie@tamby:\n"
        "  iac publish \"schema thoughts?\" --room '#db-design'\n"
        "  IAC_DIR=jamie@tamby: iac monitor --room '#db-design'\n",
        stdout);
    return 0;
}

int runPublish(const std::vector<std::string>& args)
{
    auto sender = defaultSender();
    auto cwd = currentDirectory();
    auto cwdOverridden = false;
    auto text = std::string {};

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--from" && i + 1 < args.size())
        {
            sender = args[++i];
            continue;
        }
        if (args[i] == "--cwd" && i + 1 < args.size())
        {
            cwd = args[++i];
            cwdOverridden = true;
            continue;
        }
        if (!text.empty())
            text += ' ';
        text += args[i];
    }

    if (text.empty())
        return usageError();
    if (activeRoom.remote())
    {
        // Stamp the true origin: the remote side would otherwise record its
        // own environment. Abbreviate here — our home is meaningless there.
        if (!cwdOverridden)
            cwd = shortHostname() + ":" + abbreviateHome(cwd);
        return runOverSsh({"publish", text, "--from", sender, "--cwd", cwd});
    }
    return publish(text, sender, cwd);
}

int runMonitor(const std::vector<std::string>& args)
{
    if (const auto* name = std::getenv("IAC_NAME"))
        monitorIgnore = name;
    for (std::size_t i = 1; i < args.size(); ++i)
        if (args[i] == "--ignore-from" && i + 1 < args.size())
            monitorIgnore = args[++i];

    if (activeRoom.remote())
    {
        auto forwarded = std::vector<std::string> {"monitor"};
        if (!monitorIgnore.empty())
            forwarded.insert(forwarded.end(), {"--ignore-from", monitorIgnore});
        return runOverSsh(forwarded);
    }
    return eacp::Apps::run<Monitor>();
}

int runRooms()
{
    if (activeRoom.remote())
        return runOverSsh({"rooms"});
    return listRooms();
}

int runRead(const std::vector<std::string>& args)
{
    auto count = std::size_t {20};
    for (std::size_t i = 1; i < args.size(); ++i)
        if (args[i] == "-n" && i + 1 < args.size())
            count = std::strtoul(args[++i].c_str(), nullptr, 10);

    if (activeRoom.remote())
        return runOverSsh({"read", "-n", std::to_string(count)});
    return readLast(count);
}
} // namespace iac

int main(int argc, char* argv[])
{
    auto args = iac::commandLineArguments(argc, argv);

    auto roomSpec = std::string {};
    if (const auto* env = std::getenv("IAC_DIR"))
        roomSpec = env;
    if (!roomSpec.empty())
        iac::activeRoom = iac::parseRoomSpec(roomSpec);
    for (auto it = args.begin(); it != args.end();)
    {
        if (*it == "--room" && std::next(it) != args.end())
        {
            // A bare '#name' picks a room within the store IAC_DIR already
            // names, so agents keep the store in the environment and hop
            // rooms per command; anything else is a full spec.
            const auto& spec = *std::next(it);
            if (spec.starts_with('#'))
                iac::activeRoom.name = spec.substr(1);
            else
                iac::activeRoom = iac::parseRoomSpec(spec);
            it = args.erase(it, std::next(it, 2));
            continue;
        }
        ++it;
    }
    if (!iac::activeRoom.name.empty() && !iac::validRoomName(iac::activeRoom.name))
    {
        std::fprintf(stderr,
                     "iac: invalid room name '%s' (a-z, 0-9, '-', '_')\n",
                     iac::activeRoom.name.c_str());
        return 2;
    }

    if (args.empty())
        return iac::usageError();

    const auto& command = args.front();

    if (command == "publish")
        return iac::runPublish(args);
    if (command == "read")
        return iac::runRead(args);
    if (command == "monitor")
        return iac::runMonitor(args);
    if (command == "rooms")
        return iac::runRooms();
    if (command == "help" || command == "--help" || command == "-h")
        return iac::help();

    std::fprintf(stderr, "iac: unknown command '%s'\n\n", command.c_str());
    return iac::usageError();
}
