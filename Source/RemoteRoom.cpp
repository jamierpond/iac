#include "RemoteRoom.h"

#include "Platform.h"

#include <cstdio>

namespace iac
{
namespace
{
constexpr auto sshLaunchFailedExitCode = 127;

std::string shellQuote(const std::string& text)
{
    auto quoted = std::string {"'"};
    for (const auto character: text)
    {
        if (character == '\'')
            quoted += "'\\''";
        else
            quoted += character;
    }
    return quoted + "'";
}

// The whole room — store directory plus '#name' — travels as a plain
// --room argument, so the remote side needs nothing from its
// environment; getRoomDirectory() expands a leading ~ over there.
void addForwardedRoomArgument(const Room& room, std::vector<std::string>& arguments)
{
    if (room.directory.empty() && room.name.empty())
        return;
    auto specification = room.directory;
    if (!room.name.empty())
        specification += "#" + room.name;
    arguments.insert(arguments.end(), {"--room", specification});
}

// iac usually lives in ~/.local/bin, which non-login remote shells
// don't have on PATH.
std::string buildRemoteShellCommand(const std::vector<std::string>& arguments)
{
    auto command =
        std::string {"PATH=\"$HOME/.local/bin:$PATH\"; export PATH; exec iac"};
    for (const auto& argument: arguments)
        command += ' ' + shellQuote(argument);
    return command;
}
} // namespace

std::string getShortHostname()
{
    auto host = rawHostname();
    if (const auto dot = host.find('.'); dot != std::string::npos)
        host.resize(dot);
    return host.empty() ? "remote" : host;
}

// Re-invoke iac on the room's host: the remote binary does the emberstore
// work and its stdout (one line per message) streams back over the
// connection, so even 'monitor' stays live. Callers resolve sender names and
// directories locally before forwarding — the remote side must not fall back
// to its own environment.
int runOverSsh(const Room& room, std::vector<std::string> arguments)
{
    addForwardedRoomArgument(room, arguments);
    const auto command = buildRemoteShellCommand(arguments);

    auto options = eacp::Processes::ProcessOptions {};
    options.executable = "ssh";
    options.arguments = {"-T", "-o", "ServerAliveInterval=30"};
    if (!stdinIsTty())
        options.arguments.add({"-o", "BatchMode=yes"});
    options.arguments.push_back(room.sshTarget);
    options.arguments.push_back(command);

    // Inherited stdio keeps a remote 'monitor' streaming line by line.
    options.captureOutput = false;

    const auto result = eacp::Processes::run(std::move(options));
    if (!result.launched)
    {
        std::fprintf(stderr, "iac: could not run ssh\n");
        return sshLaunchFailedExitCode;
    }
    return result.exitCode;
}
} // namespace iac
