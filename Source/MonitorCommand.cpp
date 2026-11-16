#include "Commands.h"
#include "Message.h"
#include "MessageAddressing.h"
#include "RemoteRoom.h"

#include <emberstore/FileWatcher.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <set>

namespace iac
{
namespace
{
constexpr auto decimalBase = 10;

struct MonitorSettings
{
    // This agent's name: its own messages are suppressed, and on the default
    // room only messages mentioning it print. From --ignore-from, else
    // IAC_NAME; empty disables both.
    std::string ignoredSender;
    std::size_t count = 0;
    bool printsEveryMessage = false;
};

constexpr auto printEveryMessageFlag = "--all";
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
        if (args[i] == printEveryMessageFlag)
        {
            settings.printsEveryMessage = true;
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
        std::fprintf(stderr, "iac: monitoring %s\n", document.filePath().c_str());
        if (!settings.ignoredSender.empty())
        {
            std::fprintf(stderr,
                         "iac: suppressing own messages from '%s'\n",
                         settings.ignoredSender.c_str());
        }
        if (isFilteringToAddressedMessages())
        {
            std::fprintf(stderr,
                         "iac: printing only messages that mention @%s or @all "
                         "(--all prints every message)\n",
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

    // Named breakout rooms are never filtered: whoever joined one wants all of it.
    bool isFilteringToAddressedMessages() const
    {
        return !settings.printsEveryMessage && isOnDefaultRoomWithSenderName();
    }

    bool isNotAddressedToThisAgent(const Message& message) const
    {
        return isFilteringToAddressedMessages()
               && !isMessageAddressedTo(message.text, settings.ignoredSender);
    }

    void skipMessage(const Message& message)
    {
        if (skipped.notAddressedCount == 0)
            skipped.firstSkippedTimeOfDay = formatTimeOfDay(message);
        ++skipped.notAddressedCount;
    }

    void printSkippedMessagesLine()
    {
        if (skipped.notAddressedCount == 0)
            return;
        skipped.readCountThatShowsThem = messagesSinceLastPrinted;
        std::printf("%s\n", describeSkippedMessages(skipped).c_str());
        skipped = {};
    }

    void printNewMessages()
    {
        warnIfBreakoutMatchesSender();
        for (const auto& message: cursor.takeNewMessages(document.peek()))
        {
            ++messagesSinceLastPrinted;
            if (isIgnored(message))
                continue;
            if (isNotAddressedToThisAgent(message))
            {
                skipMessage(message);
                continue;
            }
            printSkippedMessagesLine();
            printMessage(message);
            messagesSinceLastPrinted = 0;
            if (settings.count > 0 && ++printedCount >= settings.count)
            {
                eacp::Apps::quit();
                return;
            }
        }
    }

    Room room;
    MonitorSettings settings;
    std::size_t printedCount = 0;
    std::size_t messagesSinceLastPrinted = 0;
    SkippedMessages skipped;
    emberstore::Document<Messages> document =
        openRoomDatabase(room).document<Messages>(getRoomDocumentName(room));
    MessageCursor cursor {document.peek()};
    bool warnedAboutBreakout = false;
    emberstore::FileWatcher watcher {document.filePath(),
                                     [this] { printNewMessages(); }};
};

int forwardMonitorToRemoteRoom(const Room& room, const MonitorSettings& settings)
{
    auto forwarded = std::vector<std::string> {"monitor"};
    if (!settings.ignoredSender.empty())
        forwarded.insert(forwarded.end(), {"--ignore-from", settings.ignoredSender});
    if (settings.printsEveryMessage)
        forwarded.emplace_back(printEveryMessageFlag);
    if (settings.count > 0)
        forwarded.insert(forwarded.end(),
                         {"--count", std::to_string(settings.count)});
    return runOverSsh(room, forwarded);
}

// Not Apps::run<Monitor>: on Linux it mistakes an executable started through
// PATH for a plugin and returns before the loop runs.
int runMonitorUntilQuit(const Room& room, const MonitorSettings& settings)
{
    auto monitor = std::optional<Monitor> {};
    eacp::Threads::runEventLoop([&] { monitor.emplace(room, settings); });
    monitor.reset();
    return eacp::Apps::getReturnValue();
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
    return runMonitorUntilQuit(room, settings);
}
} // namespace iac
