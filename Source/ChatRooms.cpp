#include "ChatRooms.h"

#include <algorithm>
#include <iterator>

namespace iac
{
ChatRooms::ChatRooms(const std::string& activeRoomNameToUse)
    : activeRoomName(activeRoomNameToUse)
{
    getOrAddRoom(activeRoomName);
}

ChatRooms::RoomState& ChatRooms::getOrAddRoom(const std::string& name)
{
    return rooms[name];
}

void ChatRooms::addRoomWithReadHistory(const std::string& name, const Messages& history)
{
    auto& room = getOrAddRoom(name);
    room.messages = history;
    room.readCursor.markAllSeen(history);
}

void ChatRooms::updateRoomMessages(const std::string& name, const Messages& messages)
{
    auto& room = getOrAddRoom(name);
    const auto isActiveRoom = name == activeRoomName;

    // Scrolled back, the view stays on the same messages as new ones arrive below.
    if (isActiveRoom && messagesBelowView > 0)
        messagesBelowView += room.readCursor.countNewMessages(messages);

    room.messages = messages;
    if (isActiveRoom)
        room.readCursor.markAllSeen(messages);
}

void ChatRooms::selectRoom(const std::string& name)
{
    activeRoomName = name;
    auto& room = getOrAddRoom(name);
    room.readCursor.markAllSeen(room.messages);
    messagesBelowView = 0;
}

void ChatRooms::selectNeighbouringRoom(int step)
{
    const auto names = getRoomNames();
    const auto current = std::find(names.begin(), names.end(), activeRoomName);
    const auto size = static_cast<int>(names.size());
    const auto index = static_cast<int>(std::distance(names.begin(), current));
    const auto wrapped = ((index + step) % size + size) % size;
    selectRoom(names[static_cast<std::size_t>(wrapped)]);
}

const std::string& ChatRooms::getActiveRoomName() const
{
    return activeRoomName;
}

std::vector<std::string> ChatRooms::getRoomNames() const
{
    auto names = std::vector<std::string> {};
    for (const auto& [name, room]: rooms)
        names.push_back(name);
    return names;
}

std::size_t ChatRooms::getUnreadCount(const std::string& name) const
{
    const auto found = rooms.find(name);
    if (found == rooms.end())
        return 0;
    return found->second.readCursor.countNewMessages(found->second.messages);
}

const Messages& ChatRooms::getActiveRoomMessages() const
{
    return rooms.at(activeRoomName).messages;
}

std::size_t ChatRooms::getMessagesBelowView() const
{
    return messagesBelowView;
}

void ChatRooms::scrollToOlderMessages(std::size_t count)
{
    const auto messageCount = getActiveRoomMessages().size();
    const auto furthest = messageCount > 0 ? messageCount - 1 : std::size_t {0};
    messagesBelowView = std::min(messagesBelowView + count, furthest);
}

void ChatRooms::scrollToNewerMessages(std::size_t count)
{
    messagesBelowView = messagesBelowView > count ? messagesBelowView - count : 0;
}

void ChatRooms::scrollToNewestMessage()
{
    messagesBelowView = 0;
}

std::string getRoomLabel(const std::string& name)
{
    if (name.empty())
        return "(default)";
    return "#" + name;
}
} // namespace iac
