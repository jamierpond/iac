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
    std::int64_t timestamp = 0;

    MIRO_REFLECT(sender, text, timestamp)
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
    std::printf("[%s] %s: %s\n",
                formatTime(message.timestamp).c_str(),
                message.sender.c_str(),
                message.text.c_str());
    std::fflush(stdout);
}

int publish(const std::string& text, const std::string& sender)
{
    auto messages = openRoom().collection<Message>("messages");
    const auto message = Message {sender, text, nowMs()};

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

struct Monitor
{
    Monitor()
    {
        eacp::Apps::setDockIconVisible(false);
        const auto& all = document.peek();
        if (!all.empty())
            lastKey = all.rbegin()->first;
        std::fprintf(stderr, "iac: monitoring %s\n", document.filePath().c_str());
    }

    void printNewMessages()
    {
        const auto& all = document.peek();
        for (auto it = all.upper_bound(lastKey); it != all.end(); ++it)
        {
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

int usage(int code)
{
    std::fputs("iac — inter-agent chat, a local chatroom backed by emberstore\n"
               "\n"
               "usage:\n"
               "  iac publish <message...> [--from <name>]\n"
               "  iac monitor              stream incoming messages\n"
               "  iac read [-n <count>]    print the last <count> messages "
               "(default 20)\n"
               "\n"
               "environment:\n"
               "  IAC_NAME  sender name (default: $USER)\n"
               "  IAC_DIR   room directory (default: ~/.iac)\n",
               stderr);
    return code;
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
        return usage(2);
    return publish(text, sender);
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
        return iac::usage(2);

    const auto& command = args.front();

    if (command == "publish")
        return iac::runPublish(args);
    if (command == "read")
        return iac::runRead(args);
    if (command == "monitor")
        return eacp::Apps::run<iac::Monitor>();
    if (command == "help" || command == "--help" || command == "-h")
        return iac::usage(0);

    std::fprintf(stderr, "iac: unknown command '%s'\n\n", command.c_str());
    return iac::usage(2);
}
