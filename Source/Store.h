#pragma once

#include <emberstore/Emberstore.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace iac
{
struct Message
{
    std::string sender;
    std::string text;
    std::string cwd;
    std::int64_t timestamp = 0;

    MIRO_REFLECT(sender, text, cwd, timestamp)
};

using Messages = std::map<std::string, Message>;

// Messages live in a store — normally a local emberstore directory, but an
// scp-style spec ("[user@]host:dir") names one on another machine, reached
// over ssh. Within a store, a '#name' suffix picks a named room: one document
// file per room, so agents that join a breakout see it and no one else does.
struct Room
{
    std::string sshTarget;
    std::string directory;
    std::string name;

    bool remote() const { return !sshTarget.empty(); }
};

Room parseRoomSpec(const std::string& spec);
bool validRoomName(const std::string& name);

extern Room activeRoom;

eacp::FilePath roomDirectory();
emberstore::Database openRoom();
std::string roomDocumentName();
std::string defaultSender();
std::string currentDirectory();
std::string abbreviateHome(const std::string& path);
std::string formatTime(std::int64_t timestamp);
int publish(const std::string& text,
            const std::string& sender,
            const std::string& cwd);

struct RoomList
{
    bool haveDefault = false;
    std::vector<std::string> names;
};

std::optional<RoomList> findRooms();
} // namespace iac
