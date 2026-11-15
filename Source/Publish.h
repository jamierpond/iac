#pragma once

#include "Message.h"
#include "Room.h"

#include <cstdint>
#include <string>

namespace iac
{
std::int64_t getMillisecondsSinceEpoch();

// Zero-padded epoch millis so the collection's key order is chronological; a
// random suffix keeps two agents publishing in the same millisecond apart.
std::string makeMessageKey(std::int64_t timestamp);

// Writes one message into a room on this machine. False when the store can't be written.
bool publishToLocalRoom(const Room& room, const Message& message);
} // namespace iac
