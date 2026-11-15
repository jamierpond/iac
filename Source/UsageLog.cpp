#include "UsageLog.h"

#include "Platform.h"
#include "Publish.h"

#include <PondApp/DataDirectory.h>
#include <SessionLog/SessionLog.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace iac
{
namespace
{
constexpr auto sessionLogsToKeep = std::size_t {50};
constexpr auto millisecondsPerSecond = std::int64_t {1000};
constexpr auto clockTextCapacity = 32;

std::ofstream& getLogStream()
{
    static auto stream = std::ofstream {};
    return stream;
}

std::string formatClockTime(std::int64_t epochMs)
{
    const auto seconds = static_cast<std::time_t>(epochMs / millisecondsPerSecond);
    const auto local = localTime(seconds);
    const auto milliseconds = static_cast<int>(epochMs % millisecondsPerSecond);
    char text[clockTextCapacity];
    const auto length = std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    std::snprintf(text + length, sizeof(text) - length, ".%03d", milliseconds);
    return text;
}
} // namespace

void startUsageLog(std::string_view mode)
{
    const auto logsDirectory = (pondapp::getPondAppDataDirectory("iac") / "logs").string();
    auto error = std::error_code {};
    std::filesystem::create_directories(logsDirectory, error);

    const auto startedMs = getMillisecondsSinceEpoch();
    getLogStream().open(sessionlog::getSessionLogPath(logsDirectory, startedMs),
                        std::ios::app);
    sessionlog::deleteOldSessionLogs(logsDirectory, sessionLogsToKeep);
    logEvent("app.launch", {{"mode", mode}});
}

void logEvent(std::string_view event, std::initializer_list<sessionlog::Field> fields)
{
    auto& stream = getLogStream();
    if (!stream.is_open())
        return;

    stream << formatClockTime(getMillisecondsSinceEpoch()) << " | "
           << sessionlog::formatEventLine(event, fields) << '\n';
    stream.flush();
}
} // namespace iac
