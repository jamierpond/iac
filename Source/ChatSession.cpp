#include "ChatSession.h"

#include "ComposeLine.h"
#include "Publish.h"
#include "StoreRooms.h"

namespace iac
{
ChatSession::ChatSession(const Room& storeAndFirstRoom, ChatIdentity identityToUse)
    : store(storeAndFirstRoom)
    , identity(std::move(identityToUse))
    , rooms(storeAndFirstRoom.name)
{
    auto error = std::error_code {};
    auto names = findRoomNamesInStore(store, error);
    names.push_back(store.name);
    for (const auto& name: names)
        rooms.addRoomWithReadHistory(name, readRoomMessages(name));
}

Room ChatSession::getRoom(const std::string& name) const
{
    auto room = store;
    room.name = name;
    return room;
}

const Messages& ChatSession::readRoomMessages(const std::string& name)
{
    auto found = documents.find(name);
    if (found == documents.end())
    {
        const auto room = getRoom(name);
        auto document =
            openRoomDatabase(room).document<Messages>(getRoomDocumentName(room));
        found = documents.emplace(name, std::move(document)).first;
    }
    return found->second.peek();
}

void ChatSession::reloadRoomList()
{
    auto error = std::error_code {};
    for (const auto& name: findRoomNamesInStore(store, error))
    {
        if (!documents.contains(name))
            rooms.updateRoomMessages(name, readRoomMessages(name));
    }
}

void ChatSession::reloadRoom(const std::string& name)
{
    rooms.updateRoomMessages(name, readRoomMessages(name));
}

ChatSendResult ChatSession::sendComposedMessage(const std::string& text)
{
    const auto trimmed = trimComposedMessage(text);
    if (trimmed.empty())
        return {};
    const auto& name = rooms.getActiveRoomName();
    const auto message = Message {
        identity.sender, trimmed, identity.cwd, getMillisecondsSinceEpoch()};
    if (!publishToLocalRoom(getRoom(name), message))
        return {false, "could not write to " + getStoreLabel(), true};
    reloadRoom(name);
    return {true, {}, false};
}

ChatRooms& ChatSession::getRooms()
{
    return rooms;
}

const ChatRooms& ChatSession::getRooms() const
{
    return rooms;
}

const ChatIdentity& ChatSession::getIdentity() const
{
    return identity;
}

std::string ChatSession::getStoreLabel() const
{
    return abbreviateHome(getRoomDirectory(store).str());
}
} // namespace iac
