#pragma once

#include "Room.h"

#include <string>
#include <vector>

// Each command takes the full argument list, command name first, as main()
// received it with --room already removed.
namespace iac
{
int printUsageError();

int printHelp();

int runPublishCommand(const Room& room,
                      const std::vector<std::string>& arguments,
                      const std::string& defaultSender,
                      const std::string& currentDirectory);

int runReadCommand(const Room& room, const std::vector<std::string>& arguments);

int runMonitorCommand(const Room& room,
                      const std::vector<std::string>& arguments,
                      const std::string& senderNameFromEnvironment);

int runRoomsCommand(const Room& room);
} // namespace iac
