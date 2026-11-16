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

void ChatRooms::addRoomWithReadHistory(const std::string& name,
                                       const Messages& history)
{
    auto& room = getOrAddRoom(name);
    room.messages = history;
    room.readCursor.markAllSeen(history);
}

void ChatRooms::updateRoomMessages(const std::string& name, const Messages& messages)
{
    auto& room = getOrAddRoom(name);
    room.messages = messages;
    if (name == activeRoomName)
        room.readCursor.markAllSeen(messages);
}

void ChatRooms::selectRoom(const std::string& name)
{
    activeRoomName = name;
    auto& room = getOrAddRoom(name);
    room.readCursor.markAllSeen(room.messages);
}

void ChatRooms::selectRoomByOffset(int offset)
{
    const auto names = getRoomNames();
    const auto lastIndex = static_cast<int>(names.size()) - 1;
    const auto index = std::clamp(getActiveRoomIndex() + offset, 0, lastIndex);
    selectRoom(names[static_cast<std::size_t>(index)]);
}

int ChatRooms::getActiveRoomIndex() const
{
    const auto active = rooms.find(activeRoomName);
    return static_cast<int>(std::distance(rooms.begin(), active));
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

std::string getRoomLabel(const std::string& name)
{
    if (name.empty())
        return "(default)";
    return "#" + name;
}
} // namespace iac
