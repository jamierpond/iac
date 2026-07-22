// iac — inter-agent chat. A tiny local chatroom backed by emberstore: every
// message is a document in one shared collection, so any process on the
// machine can publish, read, or stream it. Rooms on other machines are
// reached by forwarding the whole invocation over ssh to the iac there.

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>
#include <emberstore/FileWatcher.h>

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

#ifdef _WIN32
    #include <io.h>
    #include <process.h>
#else
    #include <unistd.h>
#endif

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

// A room is normally a local emberstore directory, but an scp-style spec
// ("[user@]host:dir") names one on another machine, reached over ssh.
struct Room
{
    std::string sshTarget;
    std::string directory;

    bool remote() const { return !sshTarget.empty(); }
};

// scp's rule: a colon before the first slash splits host from path. An empty
// path ("host:") means the remote default room.
Room parseRoomSpec(const std::string& spec)
{
    const auto colon = spec.find(':');
    if (colon != std::string::npos && colon > 0 && spec.find('/') > colon)
        return {spec.substr(0, colon), spec.substr(colon + 1)};
    return {{}, spec};
}

Room activeRoom;

eacp::FilePath roomDirectory()
{
    if (!activeRoom.directory.empty())
        return activeRoom.directory;
    return eacp::FilePath::homeDirectory() / ".iac";
}

emberstore::Database openRoom() { return emberstore::Database {roomDirectory()}; }

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
#ifdef _WIN32
    const auto* name = std::getenv("COMPUTERNAME");
    auto host = std::string {name != nullptr ? name : ""};
#else
    char name[256] = {};
    auto host = std::string {gethostname(name, sizeof(name) - 1) == 0 ? name : ""};
#endif
    if (const auto dot = host.find('.'); dot != std::string::npos)
        host.resize(dot);
    return host.empty() ? "remote" : host;
}

bool stdinIsTty()
{
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
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
int runOverSsh(const std::vector<std::string>& arguments)
{
    // A leading ~ must expand on the remote side, where quoting would
    // defeat it — splice in "$HOME" and quote only the remainder.
    auto command = std::string {"PATH=\"$HOME/.local/bin:$PATH\"; export PATH; "};
    const auto& directory = activeRoom.directory;
    if (!directory.empty())
    {
        if (directory == "~")
            command += "IAC_DIR=\"$HOME\"; ";
        else if (directory.starts_with("~/"))
            command += "IAC_DIR=\"$HOME\"" + shellQuote(directory.substr(1)) + "; ";
        else
            command += "IAC_DIR=" + shellQuote(directory) + "; ";
        command += "export IAC_DIR; ";
    }
    command += "exec iac";
    for (const auto& argument: arguments)
        command += ' ' + shellQuote(argument);

    auto ssh =
        std::vector<std::string> {"ssh", "-T", "-o", "ServerAliveInterval=30"};
    if (!stdinIsTty())
        ssh.insert(ssh.end(), {"-o", "BatchMode=yes"});
    ssh.push_back(activeRoom.sshTarget);
    ssh.push_back(command);

    auto argv = std::vector<char*> {};
    for (auto& argument: ssh)
        argv.push_back(argument.data());
    argv.push_back(nullptr);

#ifdef _WIN32
    const auto status = _spawnvp(_P_WAIT, "ssh", argv.data());
    if (status < 0)
    {
        std::perror("iac: could not run ssh");
        return 127;
    }
    return static_cast<int>(status);
#else
    execvp("ssh", argv.data());
    std::perror("iac: could not run ssh");
    return 127;
#endif
}

std::string formatTime(std::int64_t timestamp)
{
    const auto seconds = static_cast<std::time_t>(timestamp / 1000);
    auto local = std::tm {};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
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
    auto messages = openRoom().collection<Message>("messages");
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
    auto document = openRoom().document<Messages>("messages");
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
        openRoom().document<Messages>("messages");
    std::string lastKey;
    emberstore::FileWatcher watcher {document.filePath(),
                                     [this] { printNewMessages(); }};
};

constexpr auto usageText =
    "usage:\n"
    "  iac publish <message...> [--from <name>]\n"
    "  iac monitor [--ignore-from <name>]\n"
    "  iac read [-n <count>]\n"
    "\n"
    "  every command also takes --room <dir | [user@]host:dir>\n";

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
    std::fputs("\n"
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
               "            Agents: run this under an event-driven watcher that\n"
               "            raises a notification per stdout line (e.g. Claude\n"
               "            Code's Monitor tool, persistent). A plain background\n"
               "            task only notifies on process exit — which never\n"
               "            comes — so messages pile up unread unless polled.\n"
               "  read      Print the last <count> messages, oldest first.\n"
               "              -n <count>      how many to print (default 20)\n"
               "\n"
               "rooms:\n"
               "  A room is an emberstore directory (default ~/.iac). --room, or\n"
               "  the IAC_DIR environment variable, points a command elsewhere:\n"
               "    --room /some/dir          another room on this machine\n"
               "    --room [user@]host:dir    a room on another machine, over ssh\n"
               "    --room user@host:         that machine's default room (~/.iac)\n"
               "  Remote rooms re-invoke iac on the host, so it must be installed\n"
               "  there (~/.local/bin or PATH), with key-based ssh auth — outside\n"
               "  a terminal, BatchMode is set and password prompts cannot be\n"
               "  answered. Remote publishes record their origin as 'host:~/dir'.\n"
               "\n"
               "output format:\n"
               "  [14:32:07] planner (~/projects/tamber-web): deploy is green\n"
               "\n"
               "environment:\n"
               "  IAC_NAME  sender name (default: $USER)\n"
               "  IAC_DIR   room spec, same forms as --room (default: ~/.iac).\n"
               "            Processes pointed at the same room share it; use\n"
               "            another path for a private channel.\n"
               "\n"
               "examples:\n"
               "  iac publish \"starting the migration\" --from planner\n"
               "  IAC_NAME=reviewer iac monitor\n"
               "  iac read -n 50\n"
               "  iac monitor --room jamie@tamby:\n"
               "  IAC_DIR=jamie@tamby:.iac-team iac publish \"cross-machine hi\"\n",
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
    auto args = std::vector<std::string> {argv + 1, argv + argc};

    auto roomSpec = std::string {};
    if (const auto* env = std::getenv("IAC_DIR"))
        roomSpec = env;
    for (auto it = args.begin(); it != args.end();)
    {
        if (*it == "--room" && std::next(it) != args.end())
        {
            roomSpec = *std::next(it);
            it = args.erase(it, std::next(it, 2));
            continue;
        }
        ++it;
    }
    if (!roomSpec.empty())
        iac::activeRoom = iac::parseRoomSpec(roomSpec);

    if (args.empty())
        return iac::usageError();

    const auto& command = args.front();

    if (command == "publish")
        return iac::runPublish(args);
    if (command == "read")
        return iac::runRead(args);
    if (command == "monitor")
        return iac::runMonitor(args);
    if (command == "help" || command == "--help" || command == "-h")
        return iac::help();

    std::fprintf(stderr, "iac: unknown command '%s'\n\n", command.c_str());
    return iac::usageError();
}
