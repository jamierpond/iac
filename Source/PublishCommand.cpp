#include "Commands.h"
#include "Message.h"
#include "RemoteRoom.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>

namespace iac
{
namespace
{
constexpr auto messageKeySuffixMask = 0xffffu;
constexpr auto messageKeyCapacity = 32;

struct PublishArguments
{
    std::string sender;
    std::string cwd;
    bool isCwdOverridden = false;
    std::string text;
};

PublishArguments parsePublishArguments(const std::vector<std::string>& args,
                                       const std::string& defaultSender,
                                       const std::string& currentDirectory)
{
    auto parsed = PublishArguments {defaultSender, currentDirectory};

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--from" && i + 1 < args.size())
        {
            parsed.sender = args[++i];
            continue;
        }
        if (args[i] == "--cwd" && i + 1 < args.size())
        {
            parsed.cwd = args[++i];
            parsed.isCwdOverridden = true;
            continue;
        }
        if (!parsed.text.empty())
            parsed.text += ' ';
        parsed.text += args[i];
    }
    return parsed;
}

std::int64_t getMillisecondsSinceEpoch()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

// Zero-padded epoch millis so the collection's key order is chronological; a
// random suffix keeps two agents publishing in the same millisecond apart.
std::string makeMessageKey(std::int64_t timestamp)
{
    const auto suffix = std::random_device {}() & messageKeySuffixMask;
    const auto timestampForFormat = static_cast<long long>(timestamp);
    char key[messageKeyCapacity];
    std::snprintf(key, sizeof(key), "%015lld-%04x", timestampForFormat, suffix);
    return key;
}

int publishToLocalRoom(const Room& room,
                       const std::string& text,
                       const std::string& sender,
                       const std::string& cwd)
{
    auto messages =
        openRoomDatabase(room).collection<Message>(getRoomDocumentName(room));
    const auto message = Message {sender, text, cwd, getMillisecondsSinceEpoch()};

    if (!messages.doc(makeMessageKey(message.timestamp)).set(message))
    {
        std::fprintf(
            stderr, "iac: could not write to %s\n", getRoomDirectory(room).c_str());
        return 1;
    }
    return 0;
}

int publishToRemoteRoom(const Room& room, PublishArguments parsed)
{
    // Stamp the true origin: the remote side would otherwise record its
    // own environment. Abbreviate here — our home is meaningless there.
    if (!parsed.isCwdOverridden)
        parsed.cwd = getShortHostname() + ":" + abbreviateHome(parsed.cwd);
    return runOverSsh(
        room,
        {"publish", parsed.text, "--from", parsed.sender, "--cwd", parsed.cwd});
}
} // namespace

int runPublishCommand(const Room& room,
                      const std::vector<std::string>& arguments,
                      const std::string& defaultSender,
                      const std::string& currentDirectory)
{
    const auto parsed =
        parsePublishArguments(arguments, defaultSender, currentDirectory);

    if (parsed.text.empty())
        return printUsageError();
    if (room.isRemote())
        return publishToRemoteRoom(room, parsed);
    return publishToLocalRoom(room, parsed.text, parsed.sender, parsed.cwd);
}
} // namespace iac
