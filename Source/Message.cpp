#include "Message.h"

#include "Platform.h"

#include <cstdio>
#include <ctime>

namespace iac
{
namespace
{
constexpr auto millisecondsPerSecond = std::int64_t {1000};
constexpr auto timeTextCapacity = 16;

std::string formatTime(std::int64_t timestamp)
{
    const auto seconds = static_cast<std::time_t>(timestamp / millisecondsPerSecond);
    const auto local = localTime(seconds);
    char text[timeTextCapacity];
    std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return text;
}
} // namespace

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
} // namespace iac
