#include "ChatScreen.h"
#include "Commands.h"
#include "Platform.h"
#include "RemoteRoom.h"
#include "UsageLog.h"

#include <cstdio>

namespace iac
{
namespace
{
// Chat is the human's seat in the room, so it speaks as Jamie unless told otherwise.
constexpr auto defaultChatSender = "jamie";

struct ChatArguments
{
    ChatIdentity identity;
    bool isCwdOverridden = false;
};

ChatArguments parseChatArguments(const std::vector<std::string>& args,
                                 const std::string& currentDirectory)
{
    auto parsed = ChatArguments {{defaultChatSender, currentDirectory}};
    for (std::size_t i = 1; i + 1 < args.size(); ++i)
    {
        if (args[i] == "--from")
            parsed.identity.sender = args[++i];
        else if (args[i] == "--cwd")
        {
            parsed.identity.cwd = args[++i];
            parsed.isCwdOverridden = true;
        }
    }
    return parsed;
}

// The chat screen runs on the store's host, in this terminal over ssh -t, so
// it watches the room files where they live.
int forwardChatToRemoteRoom(const Room& room, ChatArguments parsed)
{
    if (!parsed.isCwdOverridden)
        parsed.identity.cwd = getShortHostname() + ":" + abbreviateHome(parsed.identity.cwd);
    logEvent("chat.forward", {{"host", room.sshTarget}});
    return runOverSshWithTerminal(
        room, {"chat", "--from", parsed.identity.sender, "--cwd", parsed.identity.cwd});
}
} // namespace

int runChatCommand(const Room& room,
                   const std::vector<std::string>& arguments,
                   const std::string& currentDirectory)
{
    const auto parsed = parseChatArguments(arguments, currentDirectory);
    if (!stdinIsTty() || !stdoutIsTty())
    {
        logEvent("chat.error", {{"reason", "no-terminal"}});
        std::fputs("iac: chat needs a terminal\n", stderr);
        return 1;
    }
    if (room.isRemote())
        return forwardChatToRemoteRoom(room, parsed);
    return runChatScreen(room, parsed.identity);
}
} // namespace iac
