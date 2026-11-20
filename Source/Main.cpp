// iac — inter-agent chat. A tiny local chatroom backed by emberstore: every
// message is a document in a shared collection — one collection file per
// room — so any process on the machine can publish, read, or stream it.
// Stores on other machines are reached by forwarding the whole invocation
// over ssh to the iac there.

#include "Commands.h"
#include "HubConfiguration.h"
#include "Platform.h"
#include "RemoteRoom.h"
#include "UsageLog.h"

#include <PondCrashReports/CrashReports.h>
#include <PondVersion.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace iac
{
namespace
{
constexpr auto invalidRoomNameExitCode = 2;
constexpr auto unreadableConfigurationFileExitCode = 1;

std::string getDefaultSender()
{
    for (const auto* variable: {"IAC_NAME", "USER", "USERNAME"})
    {
        if (const auto value = eacp::getEnv(variable))
            return *value;
    }
    return "anon";
}

std::string getCurrentDirectory()
{
    auto error = std::error_code {};
    const auto path = std::filesystem::current_path(error);
    if (error)
        return {};
    return eacp::FilePath {path}.str();
}

// On the hub itself the configured "user@host:" names this machine, so its
// store is read directly rather than over ssh to itself.
Room readHubRoomForThisMachine()
{
    auto hub = readHubRoomFromConfigurationFile(getConfigurationFilePath());
    if (!hub)
        return {};
    if (isHubThisMachine(*hub, getShortHostname()))
        hub->sshTarget.clear();
    return *hub;
}

Room readDefaultRoom()
{
    const auto specification = eacp::getEnvValue("IAC_DIR");
    if (specification.empty())
        return readHubRoomForThisMachine();
    return parseRoomSpecification(specification);
}

bool isFullRoomSpecification(const std::string& specification)
{
    return !specification.starts_with('#');
}

// A bare '#name' picks a room within the store IAC_DIR or the hub already
// names, so agents keep the store in the environment and hop rooms per
// command; anything else is a full spec.
void applyRoomArgument(Room& room, const std::string& specification)
{
    if (isFullRoomSpecification(specification))
        room = parseRoomSpecification(specification);
    else
        room.name = specification.substr(1);
}

bool hasFullRoomSpecification(const std::vector<std::string>& specifications)
{
    return std::any_of(
        specifications.begin(), specifications.end(), isFullRoomSpecification);
}

std::vector<std::string> takeRoomSpecifications(std::vector<std::string>& args)
{
    auto specifications = std::vector<std::string> {};
    for (auto it = args.begin(); it != args.end();)
    {
        if (*it == "--room" && std::next(it) != args.end())
        {
            specifications.push_back(*std::next(it));
            it = args.erase(it, std::next(it, 2));
            continue;
        }
        ++it;
    }
    return specifications;
}

// A full --room spec replaces the default store, so the configuration file
// is not read and a broken one cannot block '--room ~/.iac'.
Room takeRoomFromArgumentsOrDefault(std::vector<std::string>& args)
{
    const auto specifications = takeRoomSpecifications(args);
    auto room = Room {};
    if (!hasFullRoomSpecification(specifications))
        room = readDefaultRoom();
    for (const auto& specification: specifications)
        applyRoomArgument(room, specification);
    return room;
}

bool isPersonAtTerminal()
{
    return stdinIsTty() && stdoutIsTty();
}

bool isVersionRequest(const std::vector<std::string>& args)
{
    return args.size() == 1 && args.front() == "--version";
}

int printVersion()
{
    std::printf("iac %s\n", pond::getVersionString().c_str());
    return 0;
}

const auto longRunningCommandNames = std::set<std::string> {"monitor", "chat"};

bool hasCrashForTestingFlag(const std::vector<std::string>& args)
{
    return std::ranges::find(args, pondcrashreports::crashForTestingFlag)
           != args.end();
}

// Starting the crash handler process adds 10 to 25 ms, several times what a
// command that prints and exits takes, so only the commands that keep running
// start it.
bool shouldStartCrashReports(const std::vector<std::string>& args)
{
    return longRunningCommandNames.contains(args.front())
           || hasCrashForTestingFlag(args);
}

void startCrashReportsIfWorthIt(const std::vector<std::string>& args,
                                int argc,
                                char* argv[])
{
    if (shouldStartCrashReports(args))
    {
        pondcrashreports::startCrashReports(
            {.appName = "iac", .release = pond::getVersionString()}, argc, argv);
    }
}

const auto helpCommandNames = std::set<std::string> {"help", "--help", "-h"};

int runPublishCommandWithEnvironmentDefaults(const Room& room,
                                             const std::vector<std::string>& args)
{
    return runPublishCommand(room, args, getDefaultSender(), getCurrentDirectory());
}

int runCommand(const Room& room, const std::vector<std::string>& args)
{
    const auto& command = args.front();

    if (command == "publish")
        return runPublishCommandWithEnvironmentDefaults(room, args);
    if (command == "read")
        return runReadCommand(room, args);
    if (command == "monitor")
        return runMonitorCommand(room, args, eacp::getEnvValue("IAC_NAME"));
    if (command == "rooms")
        return runRoomsCommand(room);
    if (command == "chat")
        return runChatCommand(room, args, getCurrentDirectory());
    if (helpCommandNames.contains(command))
        return printHelp();

    std::fprintf(stderr, "iac: unknown command '%s'\n\n", command.c_str());
    return printUsageError();
}

int runAndLogCommand(const Room& room, const std::vector<std::string>& args)
{
    startUsageLog(args.front());
    const auto exitCode = runCommand(room, args);
    logEvent("app.quit", {{"mode", args.front()}, {"exit", exitCode}});
    return exitCode;
}
} // namespace
} // namespace iac

namespace
{
int runIac(int argc, char* argv[])
{
    auto args = iac::commandLineArguments(argc, argv);

    if (iac::isVersionRequest(args))
        return iac::printVersion();

    auto room = iac::Room {};
    try
    {
        room = iac::takeRoomFromArgumentsOrDefault(args);
    }
    catch (const iac::ConfigurationFileError& error)
    {
        std::fprintf(stderr, "iac: %s\n", error.what());
        return iac::unreadableConfigurationFileExitCode;
    }
    if (!room.name.empty() && !iac::isValidRoomName(room.name))
    {
        std::fprintf(stderr,
                     "iac: invalid room name '%s' (a-z, 0-9, '-', '_')\n",
                     room.name.c_str());
        return iac::invalidRoomNameExitCode;
    }

    // Bare iac in a terminal is a person, not an agent: open the chat.
    if (args.empty() && iac::isPersonAtTerminal())
        args.emplace_back("chat");
    if (args.empty())
        return iac::printUsageError();

    iac::startCrashReportsIfWorthIt(args, argc, argv);
    return iac::runAndLogCommand(room, args);
}
} // namespace

int main(int argc, char* argv[])
{
    if (auto handlerExitCode =
            pondcrashreports::runCrashHandlerIfRequested(argc, argv))
    {
        return *handlerExitCode;
    }

    return runIac(argc, argv);
}
