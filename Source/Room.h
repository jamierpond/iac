#pragma once

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>

#include <string>
#include <string_view>

namespace iac
{
// Messages live in a store — normally a local emberstore directory, but an
// scp-style spec ("[user@]host:dir") names one on another machine, reached
// over ssh. Within a store, a '#name' suffix picks a named room: one document
// file per room, so agents that join a breakout see it and no one else does.
struct Room
{
    std::string sshTarget;
    std::string directory;
    std::string name;

    bool isRemote() const { return !sshTarget.empty(); }
};

constexpr auto defaultRoomDocumentName = std::string_view {"messages"};
constexpr auto namedRoomDocumentPrefix = std::string_view {"room-"};
constexpr auto documentFileExtension = std::string_view {".json"};

Room parseRoomSpecification(const std::string& specification);

bool isValidRoomName(const std::string& name);

eacp::FilePath getRoomDirectory(const Room& room);

emberstore::Database openRoomDatabase(const Room& room);

std::string getRoomDocumentName(const Room& room);
} // namespace iac
