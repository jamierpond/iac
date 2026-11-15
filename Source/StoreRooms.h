#pragma once

#include "Room.h"

#include <string>
#include <system_error>
#include <vector>

namespace iac
{
// The rooms that have a document in a local store, sorted, the default room
// first as "". Empty when the store can't be listed.
std::vector<std::string> findRoomNamesInStore(const Room& store, std::error_code& error);
} // namespace iac
