#pragma once

#include "Room.h"

#include <string>
#include <vector>

namespace iac
{
std::string getShortHostname();

int runOverSsh(const Room& room, std::vector<std::string> arguments);
} // namespace iac
