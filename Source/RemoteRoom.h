#pragma once

#include "Room.h"

#include <string>
#include <vector>

namespace iac
{
std::string getShortHostname();

// The -o options every forwarded invocation passes to ssh.
std::vector<std::string> getSshConnectionOptions();

// The stderr line printed when ssh exits 255: it could not connect, could not
// log in, or lost the connection.
std::string describeUnreachableHost(const Room& room);

// The --room value a forwarded invocation carries: always an explicit store,
// so the remote iac never falls back to its own IAC_DIR or hub.
std::string getForwardedRoomSpecification(const Room& room);

int runOverSsh(const Room& room, std::vector<std::string> arguments);
} // namespace iac
