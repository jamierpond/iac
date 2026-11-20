#include "Message.h"

#include "Platform.h"

#include <cstdio>
#include <ctime>
#include <iterator>

namespace iac
{
namespace
{
constexpr auto millisecondsPerSecond = std::int64_t {1000};
constexpr auto timeTextCapacity = 16;

std::string getLastKey(const Messages& messages)
{
    if (messages.empty())
        return {};
    return messages.rbegin()->first;
}
} // namespace

std::string formatTimeOfDay(const Message& message)
{
    const auto seconds =
        static_cast<std::time_t>(message.timestamp / millisecondsPerSecond);
    const auto local = localTime(seconds);
    char text[timeTextCapacity];
    std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return text;
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

std::string formatMessageLine(const Message& message)
{
    auto from = message.sender;
    if (!message.cwd.empty())
        from += " (" + abbreviateHome(message.cwd) + ")";
    return "[" + formatTimeOfDay(message) + "] " + from + ": "
           + message.text;
}

void printMessage(const Message& message)
{
    std::printf("%s\n", formatMessageLine(message).c_str());
    std::fflush(stdout);
}

std::vector<Message> getLastMessages(const Messages& messages, std::size_t count)
{
    auto last = std::vector<Message> {};
    auto skip = messages.size() > count ? messages.size() - count : std::size_t {0};
    for (const auto& [key, message]: messages)
    {
        if (skip > 0)
        {
            --skip;
            continue;
        }
        last.push_back(message);
    }
    return last;
}

MessageCursor::MessageCursor(const Messages& existing)
    : lastSeenKey(getLastKey(existing))
{
}

std::vector<Message> MessageCursor::takeNewMessages(const Messages& messages)
{
    auto newMessages = std::vector<Message> {};
    for (auto it = messages.upper_bound(lastSeenKey); it != messages.end(); ++it)
        newMessages.push_back(it->second);
    markAllSeen(messages);
    return newMessages;
}

void MessageCursor::markAllSeen(const Messages& messages)
{
    if (!messages.empty())
        lastSeenKey = getLastKey(messages);
}
} // namespace iac
