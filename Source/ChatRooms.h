#pragma once

#include "Message.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace iac
{
// What chat knows about a store's rooms, without any drawing or files: each
// room's messages, how many arrived unseen, which room is open and how far
// back it is scrolled. A room's name is "" for the default room.
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
    // added empty. Scrolling starts back at the newest message.
    void selectRoom(const std::string& name);

    // Steps through the room list, wrapping at either end.
    void selectNeighbouringRoom(int step);

    const std::string& getActiveRoomName() const;
    std::vector<std::string> getRoomNames() const;
    std::size_t getUnreadCount(const std::string& name) const;
    const Messages& getActiveRoomMessages() const;

    // How many of the open room's newest messages are below the view; 0 shows
    // the newest and keeps following new ones.
    std::size_t getMessagesBelowView() const;
    void scrollToOlderMessages(std::size_t count);
    void scrollToNewerMessages(std::size_t count);
    void scrollToNewestMessage();

private:
    struct RoomState
    {
        Messages messages;
        MessageCursor readCursor {Messages {}};
    };

    RoomState& getOrAddRoom(const std::string& name);

    std::map<std::string, RoomState> rooms;
    std::string activeRoomName;
    std::size_t messagesBelowView = 0;
};

// "#name", or "(default)" for the default room.
std::string getRoomLabel(const std::string& name);
} // namespace iac
