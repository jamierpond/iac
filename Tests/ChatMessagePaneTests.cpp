#include <ChatMessagePane.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto anyTimestamp = std::int64_t {1000};
constexpr auto wideEnough = 80;

void addMessage(Messages& messages, const std::string& key, const std::string& text)
{
    messages[key] = Message {"agent", text, {}, anyTimestamp};
}

Messages makeMessages(int count)
{
    auto messages = Messages {};
    for (auto index = 0; index < count; ++index)
        addMessage(messages,
                   "00" + std::to_string(index),
                   "message " + std::to_string(index));
    return messages;
}
} // namespace

auto tWrap = test("ChatMessagePane/wrapLine breaks at spaces, then hard") = []
{
    constexpr auto width = 10;
    constexpr auto narrowWidth = 4;
    constexpr auto hardPieces = std::size_t {3};
    auto pieces = wrapLine("the quick brown fox", width);

    check(pieces.size() == 2);
    check(pieces[0] == "the quick");
    check(pieces[1] == "brown fox");

    check(wrapLine("abcdefghij", narrowWidth).size() == hardPieces);
    check(wrapLine("", narrowWidth).size() == 1);
};

auto tWrapUtf8 = test("ChatMessagePane/wrapLine counts code points, not bytes") = []
{
    auto pieces = wrapLine("\xC3\xA9\xC3\xA9\xC3\xA9", 2);

    check(pieces.size() == 2);
    check(pieces[0] == "\xC3\xA9\xC3\xA9");
};

auto tRows =
    test("ChatMessagePane/a message is a header, its wrapped lines, then a gap") = []
{
    constexpr auto width = 10;
    auto messages = Messages {};
    addMessage(messages, "001", "the quick brown fox\nok");
    addMessage(messages, "002", "next");
    const auto rows = buildChatMessageRows(messages, width);

    constexpr auto expectedRows = std::size_t {7};
    check(rows.size() == expectedRows);
    check(rows[0].kind == ChatMessageRowKind::header);
    check(rows[1].text == "the quick");
    check(rows[2].text == "brown fox");
    check(rows[3].text == "ok");
    check(rows[4].kind == ChatMessageRowKind::gap);
    check(rows[5].kind == ChatMessageRowKind::header && rows[5].messageKey == "002");
    check(rows[6].text == "next");
};

auto tFollows =
    test("ChatMessagePane/on the newest row, the cursor follows new messages") = []
{
    auto messages = makeMessages(2);
    auto pane = ChatMessagePane {};
    pane.showRowsFromNewest(buildChatMessageRows(messages, wideEnough));
    check(pane.isFollowingNewestMessage());

    addMessage(messages, "009", "new");
    pane.replaceRows(buildChatMessageRows(messages, wideEnough));
    check(pane.isFollowingNewestMessage());
    check(pane.getRows()[static_cast<std::size_t>(pane.getCursorRow())].text
          == "new");
};

auto tHoldsPlace = test(
    "ChatMessagePane/scrolled back, the cursor stays on its message as more arrive") =
    []
{
    auto messages = makeMessages(3);
    auto pane = ChatMessagePane {};
    pane.showRowsFromNewest(buildChatMessageRows(messages, wideEnough));
    pane.jumpToRow(1);
    check(!pane.isFollowingNewestMessage());
    check(pane.countMessagesBelowCursor() == 2);

    addMessage(messages, "009", "new");
    pane.replaceRows(buildChatMessageRows(messages, wideEnough));
    check(pane.getCursorRow() == 1);
    check(pane.countMessagesBelowCursor() == 3);
};

auto tRewrap =
    test("ChatMessagePane/a narrower pane keeps the cursor on the same message") = []
{
    constexpr auto narrowWidth = 4;
    auto messages = Messages {};
    addMessage(messages, "001", "one two three four");
    addMessage(messages, "002", "last");
    auto pane = ChatMessagePane {};
    pane.showRowsFromNewest(buildChatMessageRows(messages, wideEnough));
    pane.jumpToRow(1);

    pane.replaceRows(buildChatMessageRows(messages, narrowWidth));
    const auto& row = pane.getRows()[static_cast<std::size_t>(pane.getCursorRow())];
    check(row.messageKey == "001");
    check(row.rowInMessage == 1);
};

auto tClamps =
    test("ChatMessagePane/the cursor stops at the first and last rows") = []
{
    constexpr auto farAway = 100;
    auto pane = ChatMessagePane {};
    pane.showRowsFromNewest(buildChatMessageRows(makeMessages(2), wideEnough));
    pane.moveCursor(-farAway);
    check(pane.getCursorRow() == 0);
    pane.moveCursor(farAway);
    check(pane.isFollowingNewestMessage());
};

auto tScrollMargin = test(
    "ChatMessagePane/the top row keeps a margin around the cursor, as dv's") = []
{
    constexpr auto height = 12;
    constexpr auto rowCount = 100;
    constexpr auto cursorNearBottom = 20;
    constexpr auto margin = 3;
    check(calculateTopRowShowingCursor(0, 0, height, rowCount) == 0);
    check(calculateTopRowShowingCursor(cursorNearBottom, 0, height, rowCount)
          == cursorNearBottom - height + margin + 1);
    check(calculateTopRowShowingCursor(rowCount - 1, 0, height, rowCount)
          == rowCount - height);
};
