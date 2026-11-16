#include <ChatRooms.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto anyTimestamp = std::int64_t {1000};

void addMessage(Messages& messages, const std::string& key, const std::string& text)
{
    messages[key] = Message {"agent", text, {}, anyTimestamp};
}
} // namespace

auto tHistoryIsRead = test("ChatRooms/history present at open is not unread") = []
{
    auto history = Messages {};
    addMessage(history, "001", "old");
    auto rooms = ChatRooms {""};
    rooms.addRoomWithReadHistory("design", history);
    check(rooms.getUnreadCount("design") == 0);
};

auto tUnreadInOtherRooms =
    test("ChatRooms/messages in another room count as unread until opened") = []
{
    auto messages = Messages {};
    auto rooms = ChatRooms {""};
    rooms.addRoomWithReadHistory("design", messages);

    addMessage(messages, "001", "a");
    addMessage(messages, "002", "b");
    rooms.updateRoomMessages("design", messages);
    check(rooms.getUnreadCount("design") == 2);

    rooms.selectRoom("design");
    check(rooms.getUnreadCount("design") == 0);
    check(rooms.getActiveRoomName() == "design");
};

auto tActiveRoomStaysRead =
    test("ChatRooms/messages arriving in the open room are read at once") = []
{
    auto messages = Messages {};
    auto rooms = ChatRooms {""};
    addMessage(messages, "001", "a");
    rooms.updateRoomMessages("", messages);
    check(rooms.getUnreadCount("") == 0);
    check(rooms.getActiveRoomMessages().size() == 1);
};

auto tNewRoomAllUnread =
    test("ChatRooms/a room created after opening has everything unread") = []
{
    auto messages = Messages {};
    addMessage(messages, "001", "first in a new room");
    auto rooms = ChatRooms {""};
    rooms.updateRoomMessages("fresh", messages);
    check(rooms.getUnreadCount("fresh") == 1);
};

auto tLeavingKeepsCount =
    test("ChatRooms/after leaving a room, only later messages are unread") = []
{
    auto messages = Messages {};
    addMessage(messages, "001", "seen");
    auto rooms = ChatRooms {"design"};
    rooms.updateRoomMessages("design", messages);
    rooms.selectRoom("");

    addMessage(messages, "002", "missed");
    rooms.updateRoomMessages("design", messages);
    check(rooms.getUnreadCount("design") == 1);
};

auto tRoomOrderAndOffsets = test(
    "ChatRooms/rooms list default first, sorted, and moving stops at either end") =
    []
{
    auto rooms = ChatRooms {""};
    rooms.addRoomWithReadHistory("zeta", {});
    rooms.addRoomWithReadHistory("alpha", {});
    check(rooms.getRoomNames() == std::vector<std::string> {"", "alpha", "zeta"});

    rooms.selectRoomByOffset(1);
    check(rooms.getActiveRoomName() == "alpha");
    check(rooms.getActiveRoomIndex() == 1);

    constexpr auto pastTheEnd = 5;
    rooms.selectRoomByOffset(pastTheEnd);
    check(rooms.getActiveRoomName() == "zeta");
    rooms.selectRoomByOffset(-pastTheEnd);
    check(rooms.getActiveRoomName().empty());
    check(rooms.getActiveRoomIndex() == 0);
};

auto tMessagesInKeyOrder =
    test("ChatRooms/messages are kept in publish (key) order") = []
{
    auto messages = Messages {};
    addMessage(messages, "003", "third");
    addMessage(messages, "001", "first");
    addMessage(messages, "002", "second");
    auto rooms = ChatRooms {""};
    rooms.updateRoomMessages("", messages);
    auto texts = std::vector<std::string> {};
    for (const auto& [key, message]: rooms.getActiveRoomMessages())
        texts.push_back(message.text);
    check(texts == std::vector<std::string> {"first", "second", "third"});
};
