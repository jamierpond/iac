#pragma once

#include "Message.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace iac
{
// What chat knows about a store's rooms, without any drawing or files: each
// room's messages, how many arrived unseen and which room is open. A room's
// name is "" for the default room.
class ChatRooms
{
public:
    explicit ChatRooms(const std::string& activeRoomName);

    // A room present when chat opened: its history counts as read.
    void addRoomWithReadHistory(const std::string& name, const Messages& history);

    // A room's messages after its file changed. In the open room they are
    // read at once; elsewhere they count as unread. A room unseen so far is
    // added, with everything in it unread.
    void updateRoomMessages(const std::string& name, const Messages& messages);

    // Opens the room, marking it read; a room nobody has published in yet is
    // added empty.
    void selectRoom(const std::string& name);

    // Opens the room `offset` places down the list (up when negative),
    // stopping at either end, as j/k do in DiffView's file tree.
    void selectRoomByOffset(int offset);

    const std::string& getActiveRoomName() const;
    int getActiveRoomIndex() const;
    std::vector<std::string> getRoomNames() const;
    std::size_t getUnreadCount(const std::string& name) const;
    const Messages& getActiveRoomMessages() const;

private:
    struct RoomState
    {
        Messages messages;
        MessageCursor readCursor {Messages {}};
    };

    RoomState& getOrAddRoom(const std::string& name);

    std::map<std::string, RoomState> rooms;
    std::string activeRoomName;
};

// "#name", or "(default)" for the default room.
std::string getRoomLabel(const std::string& name);
} // namespace iac
