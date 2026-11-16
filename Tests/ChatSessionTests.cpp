#include "TemporaryStore.h"

#include <ChatSession.h>
#include <Publish.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto anyTimestamp = std::int64_t {1000};

Messages readRoom(const Room& room)
{
    return openRoomDatabase(room)
        .document<Messages>(getRoomDocumentName(room))
        .read();
}
} // namespace

auto tComposePublishes = test(
    "ChatSession/a composed message is published to the open room as the chat's sender") =
    []
{
    const auto store = testing::TemporaryStore {};
    auto session = ChatSession {store.getRoom(), {"alice", "/work"}};

    const auto result = session.sendComposedMessage("hello @iac-chat\n");
    check(result.wasSent);
    check(!result.isError);

    const auto messages = readRoom(store.getRoom());
    check(messages.size() == 1);
    const auto& message = messages.begin()->second;
    check(message.sender == "alice");
    check(message.text == "hello @iac-chat");
    check(message.cwd == "/work");
    check(session.getRooms().getActiveRoomMessages().size() == 1);
};

auto tJoinThenPublish =
    test("ChatSession/after joining a room, messages go to that room only") = []
{
    const auto store = testing::TemporaryStore {};
    auto session = ChatSession {store.getRoom(), {"alice", {}}};

    session.getRooms().selectRoom("design");
    check(session.getRooms().getActiveRoomName() == "design");
    session.sendComposedMessage("in design");

    check(readRoom(store.getRoom("design")).size() == 1);
    check(readRoom(store.getRoom()).empty());
};

auto tOtherPublishersCountUnread = test(
    "ChatSession/another agent's publish shows as unread in a room not open") = []
{
    const auto store = testing::TemporaryStore {};
    check(publishToLocalRoom(store.getRoom("ops"),
                             Message {"x", "before", {}, anyTimestamp}));
    auto session = ChatSession {store.getRoom(), {"alice", {}}};
    check(session.getRooms().getUnreadCount("ops") == 0);

    check(publishToLocalRoom(store.getRoom("ops"),
                             Message {"x", "after", {}, anyTimestamp + 1}));
    session.reloadRoom("ops");
    check(session.getRooms().getUnreadCount("ops") == 1);
};

auto tNewRoomFound = test(
    "ChatSession/a room created by someone else appears with its messages unread") =
    []
{
    const auto store = testing::TemporaryStore {};
    auto session = ChatSession {store.getRoom(), {"alice", {}}};
    check(publishToLocalRoom(store.getRoom("fresh"),
                             Message {"x", "hi", {}, anyTimestamp}));

    session.reloadRoomList();
    const auto names = session.getRooms().getRoomNames();
    check(std::find(names.begin(), names.end(), "fresh") != names.end());
    check(session.getRooms().getUnreadCount("fresh") == 1);
};

auto tBlankSendsNothing =
    test("ChatSession/blank text in the compose box publishes nothing") = []
{
    const auto store = testing::TemporaryStore {};
    auto session = ChatSession {store.getRoom(), {"alice", {}}};
    const auto result = session.sendComposedMessage(" \n ");
    check(!result.wasSent);
    check(!result.isError);
    check(readRoom(store.getRoom()).empty());
};
