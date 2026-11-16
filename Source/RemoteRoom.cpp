#include "RemoteRoom.h"

#include "Platform.h"

#include <cstdio>

namespace iac
{
namespace
{
constexpr auto sshLaunchFailedExitCode = 127;
constexpr auto sshConnectionFailedExitCode = 255;
constexpr auto sshConnectTimeoutSeconds = 5;
constexpr auto sshServerAliveIntervalSeconds = 30;
constexpr auto homeRelativePrefix = std::string_view {"~/"};

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

void addForwardedRoomArgument(const Room& room, std::vector<std::string>& arguments)
{
    arguments.insert(arguments.end(),
                     {"--room", getForwardedRoomSpecification(room)});
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

eacp::Processes::ProcessOptions makeSshOptions(const Room& room,
                                               std::vector<std::string> arguments,
                                               const char* terminalFlag)
{
    addForwardedRoomArgument(room, arguments);
    auto options = eacp::Processes::ProcessOptions {};
    options.executable = "ssh";
    options.arguments = {terminalFlag};
    for (auto& option: getSshConnectionOptions())
        options.arguments.push_back(option);
    if (!stdinIsTty())
        options.arguments.add({"-o", "BatchMode=yes"});
    options.arguments.push_back(room.sshTarget);
    options.arguments.push_back(buildRemoteShellCommand(arguments));

    // Inherited stdio keeps a remote 'monitor' streaming line by line.
    options.captureOutput = false;
    return options;
}

// ssh exits 255 for its own failures; iac never exits 255 itself, so this
// is ssh failing, not the remote iac.
int runSsh(const Room& room, eacp::Processes::ProcessOptions options)
{
    const auto result = eacp::Processes::run(std::move(options));
    if (!result.launched)
    {
        std::fprintf(stderr, "iac: could not run ssh\n");
        return sshLaunchFailedExitCode;
    }
    if (result.exitCode == sshConnectionFailedExitCode)
        std::fprintf(stderr, "%s\n", describeUnreachableHost(room).c_str());
    return result.exitCode;
}
} // namespace

// getRoomDirectory() expands the leading ~ on the remote side.
std::string getForwardedRoomSpecification(const Room& room)
{
    auto specification = room.directory;
    if (specification.empty())
    {
        specification = std::string {homeRelativePrefix}
                        + std::string {defaultStoreDirectoryName};
    }
    if (!room.name.empty())
        specification += "#" + room.name;
    return specification;
}

std::vector<std::string> getSshConnectionOptions()
{
    return {"-o",
            "ConnectTimeout=" + std::to_string(sshConnectTimeoutSeconds),
            "-o",
            "ServerAliveInterval=" + std::to_string(sshServerAliveIntervalSeconds)};
}

std::string describeUnreachableHost(const Room& room)
{
    return "iac: ssh to " + room.sshTarget + " failed (no connection within "
           + std::to_string(sshConnectTimeoutSeconds)
           + " s, a refused login, or a dropped link); the command did not finish"
             " on that host. This machine's own room is reached with --room ~/.iac.";
}

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
    return runSsh(room, makeSshOptions(room, std::move(arguments), "-T"));
}

int runOverSshWithTerminal(const Room& room, std::vector<std::string> arguments)
{
    return runSsh(room, makeSshOptions(room, std::move(arguments), "-t"));
}
} // namespace iac
