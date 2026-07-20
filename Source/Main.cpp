// iac — inter-agent chat. A tiny local chatroom backed by emberstore: every
// message is a document in one shared collection, so any process on the
// machine can publish, read, or stream it.

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>
#include <emberstore/FileWatcher.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
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

eacp::FilePath roomDirectory()
{
    if (const auto* custom = std::getenv("IAC_DIR"))
        return custom;
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

int publish(const std::string& text, const std::string& sender)
{
    auto messages = openRoom().collection<Message>("messages");
    const auto message = Message {sender, text, currentDirectory(), nowMs()};

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
            std::fprintf(
                stderr, "iac: suppressing own messages from '%s'\n", monitorIgnore.c_str());
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

constexpr auto usageText = "usage:\n"
                           "  iac publish <message...> [--from <name>]\n"
                           "  iac monitor [--ignore-from <name>]\n"
                           "  iac read [-n <count>]\n";

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
               "A local chatroom for the coding agents (and humans) on this\n"
               "machine. Any process can post to the room, stream it live,\n"
               "or read back its history.\n"
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
               "output format:\n"
               "  [14:32:07] planner (~/projects/tamber-web): deploy is green\n"
               "\n"
               "environment:\n"
               "  IAC_NAME  sender name (default: $USER)\n"
               "  IAC_DIR   room directory (default: ~/.iac). Processes pointed\n"
               "            at the same directory share a room; use another path\n"
               "            for a private channel.\n"
               "\n"
               "examples:\n"
               "  iac publish \"starting the migration\" --from planner\n"
               "  IAC_NAME=reviewer iac monitor\n"
               "  iac read -n 50\n",
               stdout);
    return 0;
}

int runPublish(const std::vector<std::string>& args)
{
    auto sender = defaultSender();
    auto text = std::string {};

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--from" && i + 1 < args.size())
        {
            sender = args[++i];
            continue;
        }
        if (!text.empty())
            text += ' ';
        text += args[i];
    }

    if (text.empty())
        return usageError();
    return publish(text, sender);
}

int runMonitor(const std::vector<std::string>& args)
{
    if (const auto* name = std::getenv("IAC_NAME"))
        monitorIgnore = name;
    for (std::size_t i = 1; i < args.size(); ++i)
        if (args[i] == "--ignore-from" && i + 1 < args.size())
            monitorIgnore = args[++i];
    return eacp::Apps::run<Monitor>();
}

int runRead(const std::vector<std::string>& args)
{
    auto count = std::size_t {20};
    for (std::size_t i = 1; i < args.size(); ++i)
        if (args[i] == "-n" && i + 1 < args.size())
            count = std::strtoul(args[++i].c_str(), nullptr, 10);
    return readLast(count);
}
} // namespace iac

int main(int argc, char* argv[])
{
    const auto args = std::vector<std::string> {argv + 1, argv + argc};
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
