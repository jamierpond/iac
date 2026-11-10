#include "Commands.h"
#include "Message.h"
#include "RemoteRoom.h"

#include <emberstore/FileWatcher.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>

namespace iac
{
namespace
{
constexpr auto decimalBase = 10;

struct MonitorSettings
{
    // Sender whose messages 'monitor' suppresses, so a session is never woken
    // by its own publishes. From --ignore-from, else IAC_NAME; empty disables.
    std::string ignoredSender;
    std::size_t count = 0;
};

const auto onceFlags = std::set<std::string> {"--once", "-1"};
const auto countFlags = std::set<std::string> {"-n", "--count"};

MonitorSettings parseMonitorArguments(const std::vector<std::string>& args,
                                      const std::string& senderNameFromEnvironment)
{
    auto settings = MonitorSettings {senderNameFromEnvironment};
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const auto hasValue = i + 1 < args.size();
        if (args[i] == "--ignore-from" && hasValue)
        {
            settings.ignoredSender = args[++i];
            continue;
        }
        if (onceFlags.contains(args[i]))
        {
            settings.count = 1;
            continue;
        }
        if (countFlags.contains(args[i]) && hasValue)
        {
            settings.count = std::strtoul(args[++i].c_str(), nullptr, decimalBase);
            continue;
        }
    }
    return settings;
}

struct Monitor
{
    Monitor(const Room& roomToWatch, const MonitorSettings& settingsToUse)
        : room(roomToWatch)
        , settings(settingsToUse)
    {
        eacp::Apps::setDockIconVisible(false);
        const auto& all = document.peek();
        if (!all.empty())
            lastKey = all.rbegin()->first;
        std::fprintf(stderr, "iac: monitoring %s\n", document.filePath().c_str());
        if (!settings.ignoredSender.empty())
        {
            std::fprintf(stderr,
                         "iac: suppressing own messages from '%s'\n",
                         settings.ignoredSender.c_str());
        }
        warnIfBreakoutMatchesSender();
    }

    // A monitor on the default room whose sender name is also the name of a
    // breakout room is almost always an agent told to "join chat X" that put X
    // in IAC_NAME instead of --room '#X': everything published into #X passes
    // it by. Said once, on stdout, so a harness streaming this monitor wakes
    // the agent with the hint - and checked again on every wake, since the
    // room usually appears after the monitor started.
    void warnIfBreakoutMatchesSender()
    {
        if (warnedAboutBreakout || !isOnDefaultRoomWithSenderName())
            return;
        const auto& sender = settings.ignoredSender;
        const auto breakoutFileName = std::string {namedRoomDocumentPrefix} + sender
                                      + std::string {documentFileExtension};
        const auto breakout = getRoomDirectory(room) / breakoutFileName;
        if (!std::filesystem::exists(breakout.c_str()))
            return;
        warnedAboutBreakout = true;
        std::printf("iac: a breakout room '#%s' exists, but this monitor is on the "
                    "default room with '%s' as its sender name, so messages "
                    "published there do not reach it. If that room is meant for "
                    "you, rejoin with: iac monitor --room '#%s'\n",
                    sender.c_str(),
                    sender.c_str(),
                    sender.c_str());
        std::fflush(stdout);
    }

    bool isOnDefaultRoomWithSenderName() const
    {
        return !settings.ignoredSender.empty() && room.name.empty();
    }

    bool isIgnored(const Message& message) const
    {
        return !settings.ignoredSender.empty()
               && message.sender == settings.ignoredSender;
    }

    void printNewMessages()
    {
        warnIfBreakoutMatchesSender();
        const auto& all = document.peek();
        for (auto it = all.upper_bound(lastKey); it != all.end(); ++it)
        {
            if (!isIgnored(it->second))
            {
                printMessage(it->second);
                if (settings.count > 0 && ++printedCount >= settings.count)
                {
                    eacp::Apps::quit();
                    return;
                }
            }
            lastKey = it->first;
        }
    }

    Room room;
    MonitorSettings settings;
    std::size_t printedCount = 0;
    emberstore::Document<Messages> document =
        openRoomDatabase(room).document<Messages>(getRoomDocumentName(room));
    std::string lastKey;
    bool warnedAboutBreakout = false;
    emberstore::FileWatcher watcher {document.filePath(),
                                     [this] { printNewMessages(); }};
};

int forwardMonitorToRemoteRoom(const Room& room, const MonitorSettings& settings)
{
    auto forwarded = std::vector<std::string> {"monitor"};
    if (!settings.ignoredSender.empty())
        forwarded.insert(forwarded.end(), {"--ignore-from", settings.ignoredSender});
    if (settings.count > 0)
        forwarded.insert(forwarded.end(),
                         {"--count", std::to_string(settings.count)});
    return runOverSsh(room, forwarded);
}
} // namespace

int runMonitorCommand(const Room& room,
                      const std::vector<std::string>& arguments,
                      const std::string& senderNameFromEnvironment)
{
    const auto settings =
        parseMonitorArguments(arguments, senderNameFromEnvironment);

    if (room.isRemote())
        return forwardMonitorToRemoteRoom(room, settings);
    return eacp::Apps::run<Monitor>(room, settings);
}
} // namespace iac
