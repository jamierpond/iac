#pragma once

#include "ChatRooms.h"
#include "ComposeLine.h"
#include "Room.h"

#include <map>
#include <string>
#include <vector>

namespace iac
{
// Who chat publishes as: the sender name and the directory recorded with each message.
struct ChatIdentity
{
    std::string sender;
    std::string cwd;
};

// What submitting the prompt did, for the screen to act on: quit, or show `status`.
struct ChatSubmitResult
{
    ComposeActionKind kind = ComposeActionKind::nothing;
    std::string status;
    bool isError = false;
};

// Chat against one local store: reads its rooms into ChatRooms and publishes
// what's typed. No drawing and no watching: the screen calls reloadRoom when a
// room's file changes and reloadRoomList now and then for new rooms.
class ChatSession
{
public:
    ChatSession(const Room& storeAndFirstRoom, ChatIdentity identityToUse);

    void reloadRoomList();
    void reloadRoom(const std::string& name);

    ChatSubmitResult submitComposedText(const std::string& text);

    ChatRooms& getRooms();
    const ChatRooms& getRooms() const;
    const ChatIdentity& getIdentity() const;
    Room getRoom(const std::string& name) const;
    std::string getStoreLabel() const;

private:
    const Messages& readRoomMessages(const std::string& name);
    ChatSubmitResult publishToActiveRoom(const std::string& text);
    ChatSubmitResult describeRooms() const;

    Room store;
    ChatIdentity identity;
    ChatRooms rooms;
    std::map<std::string, emberstore::Document<Messages>> documents;
};
} // namespace iac
