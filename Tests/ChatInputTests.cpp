#include "TemporaryStore.h"

#include <ChatInput.h>
#include <Publish.h>

#include <NanoTest/NanoTest.h>

#include <initializer_list>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto anyTimestamp = std::int64_t {1000};
constexpr auto roomsHeight = 10;
constexpr auto messagesHeight = 6;
constexpr auto wrapWidth = 60;

Messages readRoom(const Room& room)
{
    return openRoomDatabase(room)
        .document<Messages>(getRoomDocumentName(room))
        .read();
}

void typeKeys(ChatInput& input, std::initializer_list<const char*> keys)
{
    for (const auto* key: keys)
        input.handleKey(key);
}

void publishMessages(const Room& room,
                     int count,
                     std::int64_t firstTimestamp = anyTimestamp)
{
    for (auto index = 0; index < count; ++index)
        check(publishToLocalRoom(
            room, Message {"agent", "hi", {}, firstTimestamp + index}));
}

// A store with the default room and #alpha and #beta, opened at the default room.
struct ChatWithThreeRooms
{
    ChatWithThreeRooms()
    {
        publishMessages(store.getRoom("alpha"), 1);
        publishMessages(store.getRoom("beta"), 1);
        session.emplace(store.getRoom(), ChatIdentity {"alice", {}});
        input.emplace(*session);
        input->layOutPanes({roomsHeight, messagesHeight, wrapWidth});
    }

    const std::string& getOpenRoom() const
    {
        return session->getRooms().getActiveRoomName();
    }

    testing::TemporaryStore store;
    std::optional<ChatSession> session;
    std::optional<ChatInput> input;
};
} // namespace

auto tStartsInRooms =
    test("ChatInput/opens on the rooms list, not in the compose box") = []
{
    auto chat = ChatWithThreeRooms {};
    check(chat.input->getFocusedPane() == ChatPane::rooms);
    check(!chat.input->isComposing());
};

auto tVimKeysPublishNothing =
    test("ChatInput/vim keys typed on opening publish nothing") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"a", ":", "q", "l", "Esc"});
    check(readRoom(chat.store.getRoom()).empty());
    check(!chat.input->hasQuit());
};

auto tRoomsMoveLikeTree = test(
    "ChatInput/j and k open the next and previous room, stopping at the ends") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"j"});
    check(chat.getOpenRoom() == "alpha");
    typeKeys(*chat.input, {"j", "j", "j"});
    check(chat.getOpenRoom() == "beta");
    typeKeys(*chat.input, {"g", "g"});
    check(chat.getOpenRoom().empty());
    typeKeys(*chat.input, {"G"});
    check(chat.getOpenRoom() == "beta");
    typeKeys(*chat.input, {"2", "k"});
    check(chat.getOpenRoom().empty());
};

auto tMovingReadsRoom =
    test("ChatInput/a room the cursor lands on counts as read") = []
{
    auto chat = ChatWithThreeRooms {};
    constexpr auto afterOpening = anyTimestamp * 2;
    publishMessages(chat.store.getRoom("alpha"), 2, afterOpening);
    chat.input->reloadRoom("alpha");
    check(chat.session->getRooms().getUnreadCount("alpha") == 2);
    typeKeys(*chat.input, {"j"});
    check(chat.session->getRooms().getUnreadCount("alpha") == 0);
};

auto tFocusLikeDiffView =
    test("ChatInput/Enter and l open the messages, h goes back, Tab switches") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"Enter"});
    check(chat.input->getFocusedPane() == ChatPane::messages);
    typeKeys(*chat.input, {"h"});
    check(chat.input->getFocusedPane() == ChatPane::rooms);
    typeKeys(*chat.input, {"l"});
    check(chat.input->getFocusedPane() == ChatPane::messages);
    typeKeys(*chat.input, {"Tab"});
    check(chat.input->getFocusedPane() == ChatPane::rooms);
    check(chat.getOpenRoom().empty());
};

auto tMessagesScroll =
    test("ChatInput/in the messages, j k C-u G move the cursor by rows") = []
{
    constexpr auto messageCount = 10;
    auto chat = ChatWithThreeRooms {};
    publishMessages(chat.store.getRoom(), messageCount);
    chat.input->reloadRoom("");
    const auto& pane = chat.input->getMessagePane();
    const auto lastRow = static_cast<int>(pane.getRows().size()) - 1;
    check(pane.getCursorRow() == lastRow);

    typeKeys(*chat.input, {"l", "k"});
    check(pane.getCursorRow() == lastRow - 1);
    typeKeys(*chat.input, {"C-u"});
    check(pane.getCursorRow() == lastRow - 1 - messagesHeight / 2);
    typeKeys(*chat.input, {"g", "g"});
    check(pane.getCursorRow() == 0);
    typeKeys(*chat.input, {"G"});
    check(pane.isFollowingNewestMessage());
};

auto tComposeAndSend =
    test("ChatInput/c opens the box in insert mode; Ctrl-S sends and closes it") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"c"});
    check(chat.input->isComposing());
    check(chat.input->getEditor().mode() == vim::Mode::insert);
    typeKeys(*chat.input, {"h", "i", "Enter", "y", "o"});
    check(chat.input->isComposing());
    typeKeys(*chat.input, {"C-s"});
    check(!chat.input->isComposing());

    const auto messages = readRoom(chat.store.getRoom());
    check(messages.size() == 1);
    check(messages.begin()->second.text == "hi\nyo");
    check(chat.input->getSentCount() == 1);
};

auto tWriteSends =
    test("ChatInput/:w sends and keeps the box open; :q! drops the text") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"c", "x", "Esc", ":", "w", "Enter"});
    check(readRoom(chat.store.getRoom()).size() == 1);
    check(chat.input->isComposing());

    typeKeys(*chat.input, {"i", "y", "Esc", ":", "q", "!", "Enter"});
    check(!chat.input->isComposing());
    check(!chat.input->hasUnsentDraft());
    check(readRoom(chat.store.getRoom()).size() == 1);
};

auto tDraftSurvives =
    test("ChatInput/Esc twice leaves the box and keeps the draft for c") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"c", "h", "a", "l", "f", "Esc", "Esc"});
    check(!chat.input->isComposing());
    check(chat.input->hasUnsentDraft());
    check(readRoom(chat.store.getRoom()).empty());

    typeKeys(*chat.input, {"c"});
    check(chat.input->isComposing());
    check(chat.input->getEditor().mode() == vim::Mode::normal);
    check(chat.input->getEditor().text() == "half");

    typeKeys(*chat.input, {"Esc", "i"});
    check(chat.input->getEditor().mode() == vim::Mode::insert);
    typeKeys(*chat.input, {"!", "C-s"});
    check(readRoom(chat.store.getRoom()).begin()->second.text == "half!");
};

auto tJoinOpensNewRoom =
    test("ChatInput/:join #room opens a room nobody has used yet") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input,
             {":", "j", "o", "i", "n", " ", "#", "n", "e", "w", "Enter"});
    check(!chat.input->isCommandLineOpen());
    check(chat.getOpenRoom() == "new");

    typeKeys(*chat.input, {"c", "o", "k", "C-s"});
    check(readRoom(chat.store.getRoom("new")).size() == 1);
};

auto tCommandLineCancel =
    test("ChatInput/Esc closes the command line without running it") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {":", "q", "Esc"});
    check(!chat.input->isCommandLineOpen());
    check(!chat.input->hasQuit());
    typeKeys(*chat.input, {":", "q", "Enter"});
    check(chat.input->hasQuit());
};

auto tQuit = test("ChatInput/q quits from the panes") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"q"});
    check(chat.input->hasQuit());
};

auto tHelp = test("ChatInput/F1 shows the keys and any key closes them") = []
{
    auto chat = ChatWithThreeRooms {};
    typeKeys(*chat.input, {"F1"});
    check(chat.input->isShowingHelp());
    typeKeys(*chat.input, {"q"});
    check(!chat.input->isShowingHelp());
    check(!chat.input->hasQuit());
};
