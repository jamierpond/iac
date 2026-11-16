#pragma once

#include "Room.h"

#include <string>
#include <vector>

namespace iac
{
std::string getShortHostname();

// The --room value a forwarded invocation carries: always an explicit store,
// so the remote iac never falls back to its own IAC_DIR or hub.
std::string getForwardedRoomSpecification(const Room& room);

int runOverSsh(const Room& room, std::vector<std::string> arguments);

// Like runOverSsh, but with a terminal on the remote side (ssh -t), for a
// full-screen command such as chat.
int runOverSshWithTerminal(const Room& room, std::vector<std::string> arguments);
} // namespace iac
