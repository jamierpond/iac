// iac — inter-agent chat. A tiny local chatroom backed by emberstore: every
// message is a document in a shared collection — one collection file per
// room — so any process on the machine can publish, read, or stream it.
// Stores on other machines are reached by forwarding the whole invocation
// over ssh to the iac there.

#include "Commands.h"
#include "Platform.h"

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

Room readRoomFromEnvironment()
{
    const auto specification = eacp::getEnvValue("IAC_DIR");
    if (specification.empty())
        return {};
    return parseRoomSpecification(specification);
}

// A bare '#name' picks a room within the store IAC_DIR already names, so
// agents keep the store in the environment and hop rooms per command; anything
// else is a full spec.
void applyRoomArgument(Room& room, const std::string& specification)
{
    if (specification.starts_with('#'))
        room.name = specification.substr(1);
    else
        room = parseRoomSpecification(specification);
}

void takeRoomArguments(std::vector<std::string>& args, Room& room)
{
    for (auto it = args.begin(); it != args.end();)
    {
        if (*it == "--room" && std::next(it) != args.end())
        {
            applyRoomArgument(room, *std::next(it));
            it = args.erase(it, std::next(it, 2));
            continue;
        }
        ++it;
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
    if (helpCommandNames.contains(command))
        return printHelp();

    std::fprintf(stderr, "iac: unknown command '%s'\n\n", command.c_str());
    return printUsageError();
}
} // namespace
} // namespace iac

int main(int argc, char* argv[])
{
    auto args = iac::commandLineArguments(argc, argv);

    auto room = iac::readRoomFromEnvironment();
    iac::takeRoomArguments(args, room);
    if (!room.name.empty() && !iac::isValidRoomName(room.name))
    {
        std::fprintf(stderr,
                     "iac: invalid room name '%s' (a-z, 0-9, '-', '_')\n",
                     room.name.c_str());
        return iac::invalidRoomNameExitCode;
    }

    if (args.empty())
        return iac::printUsageError();

    return iac::runCommand(room, args);
}
