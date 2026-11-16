#include "ChatView.h"

#include "ComposeLine.h"
#include "Publish.h"

#include <EditorText.h>

#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace iac
{
namespace
{
using namespace ftxui;
using ftxui::text;

constexpr auto roomsPaneWidthPercent = 30;
constexpr auto percent = 100;
constexpr auto roomsPaneMinimumWidth = 20;
constexpr auto roomsPaneMaximumWidth = 50;
constexpr auto paneSeparatorWidth = 1;
constexpr auto messagesPaneMinimumWidth = 10;
// The panes' title row and the status line.
constexpr auto rowsAroundPanes = 2;
constexpr auto messageTextIndent = "   ";
constexpr auto messageTextIndentWidth = 3;
constexpr auto composeBoxMinimumTextRows = 3;
constexpr auto composeBoxMaximumTextRows = 8;
// Two borders and the mode line under the text.
constexpr auto composeBoxChromeRows = 3;
constexpr auto composeBoxBorderWidth = 2;
constexpr auto messageRowsKeptBesideComposeBox = 2;
constexpr auto statusShownForMs = std::int64_t {5000};
constexpr auto helpWidth = 64;
constexpr auto helpKeyColumnWidth = 18;
constexpr auto fnvOffsetBasis = std::uint32_t {2166136261u};
constexpr auto fnvPrime = std::uint32_t {16777619u};

// 256-colour palette entries that read on dark and light terminals alike.
constexpr auto senderPalette = std::array<std::uint8_t, 16> {
    33, 37, 71, 107, 133, 136, 166, 172, 99, 161, 30, 64, 169, 32, 208, 139};

const auto accentColor = Color::Yellow;
const auto unreadColor = Color::Cyan;
const auto focusedCursorBackground = Color::RGB(58, 62, 84);
const auto unfocusedCursorBackground = Color::RGB(40, 42, 54);

struct ChatLayout
{
    int roomsWidth = 0;
    int messagesWidth = 0;
    int bodyHeight = 0;
    int composeBoxHeight = 0;
};

// FNV-1a rather than std::hash, so an agent keeps its colour across runs and platforms.
Color getSenderColor(const std::string& sender)
{
    auto hash = fnvOffsetBasis;
    for (const auto character: sender)
    {
        hash ^= static_cast<unsigned char>(character);
        hash *= fnvPrime;
    }
    const auto paletteIndex = hash % senderPalette.size();
    return static_cast<Color::Palette256>(senderPalette[paletteIndex]);
}

// DiffView's cursorLine: a dark background rather than inverted, so colours stay readable.
Element highlightCursorRow(Element row, bool isPaneFocused)
{
    if (isPaneFocused)
        return std::move(row) | bgcolor(focusedCursorBackground);
    return std::move(row) | bgcolor(unfocusedCursorBackground);
}

Element renderPaneTitle(const std::string& title, bool isPaneFocused)
{
    auto shown = text(title) | bold;
    if (isPaneFocused)
        return shown | inverted;
    return shown | dim;
}

int calculateComposeBoxHeight(vim::Editor& editor, int messagesWidth, int bodyHeight)
{
    editor.setWrapWidth(std::max(1, messagesWidth - composeBoxBorderWidth));
    const auto textRows = std::clamp(static_cast<int>(editor.visualRows().size()),
                                     composeBoxMinimumTextRows,
                                     composeBoxMaximumTextRows);
    const auto tallestBox = bodyHeight - messageRowsKeptBesideComposeBox;
    return std::max(composeBoxChromeRows + 1,
                    std::min(textRows + composeBoxChromeRows, tallestBox));
}

ChatLayout calculateChatLayout(ChatInput& input)
{
    const auto terminal = Terminal::Size();
    auto layout = ChatLayout {};
    layout.roomsWidth = std::clamp(terminal.dimx * roomsPaneWidthPercent / percent,
                                   roomsPaneMinimumWidth,
                                   roomsPaneMaximumWidth);
    layout.messagesWidth =
        std::max(messagesPaneMinimumWidth,
                 terminal.dimx - layout.roomsWidth - paneSeparatorWidth);
    layout.bodyHeight = std::max(1, terminal.dimy - rowsAroundPanes);
    if (input.isComposing())
    {
        layout.composeBoxHeight = calculateComposeBoxHeight(
            input.getEditor(), layout.messagesWidth, layout.bodyHeight);
    }
    return layout;
}

ChatPaneSizes getPaneSizes(const ChatLayout& layout)
{
    return {layout.bodyHeight,
            std::max(1, layout.bodyHeight - layout.composeBoxHeight),
            std::max(1, layout.messagesWidth - messageTextIndentWidth)};
}

Element renderRoomRow(const ChatRooms& rooms, const std::string& name)
{
    auto parts = Elements {text(" " + getRoomLabel(name)), filler()};
    const auto unread = rooms.getUnreadCount(name);
    if (unread > 0)
        parts.push_back(text(" ●" + std::to_string(unread) + " ")
                        | color(unreadColor));
    return hbox(std::move(parts));
}

Element renderRoomsPane(const ChatInput& input, const ChatLayout& layout)
{
    const auto& rooms = input.getSession().getRooms();
    const auto names = rooms.getRoomNames();
    const auto isFocused = input.getFocusedPane() == ChatPane::rooms;
    auto lines = Elements {renderPaneTitle(
        " Rooms · " + std::to_string(names.size()) + " ", isFocused)};

    const auto first = input.getRoomsTopRow();
    const auto end =
        std::min(static_cast<int>(names.size()), first + layout.bodyHeight);
    for (auto index = first; index < end; ++index)
    {
        const auto& name = names[static_cast<std::size_t>(index)];
        auto row = renderRoomRow(rooms, name);
        if (index == rooms.getActiveRoomIndex())
            row = highlightCursorRow(row, isFocused);
        lines.push_back(row | size(WIDTH, LESS_THAN, layout.roomsWidth + 1));
    }
    return vbox(std::move(lines)) | flex;
}

bool isMentionOf(std::string_view mention, const std::string& sender)
{
    return mention.substr(1) == sender;
}

// One wrapped piece of a message with each "@role" coloured, and a mention of
// the person chatting shown inverted.
Element renderMessageText(const std::string& line, const std::string& reader)
{
    auto pieces = Elements {text(messageTextIndent)};
    auto position = std::size_t {0};
    for (const auto& mention: findMentions(line))
    {
        pieces.push_back(text(line.substr(position, mention.start - position)));
        const auto mentionText = line.substr(mention.start, mention.length);
        auto shown = text(mentionText) | bold | color(accentColor);
        if (isMentionOf(mentionText, reader))
            shown = shown | inverted;
        pieces.push_back(shown);
        position = mention.start + mention.length;
    }
    pieces.push_back(text(line.substr(position)));
    pieces.push_back(filler());
    return hbox(std::move(pieces));
}

Element renderMessageHeader(const Message& message)
{
    auto parts = Elements {text(" " + formatTimeOfDay(message)) | dim,
                           text(" "),
                           text(message.sender) | bold
                               | color(getSenderColor(message.sender))};
    if (!message.cwd.empty())
        parts.push_back(text(" (" + abbreviateHome(message.cwd) + ")") | dim);
    parts.push_back(filler());
    return hbox(std::move(parts));
}

Element renderMessageRow(const ChatMessageRow& row,
                         const Messages& messages,
                         const std::string& reader)
{
    switch (row.kind)
    {
        case ChatMessageRowKind::header:
            return renderMessageHeader(messages.at(row.messageKey));
        case ChatMessageRowKind::text:
            return renderMessageText(row.text, reader);
        case ChatMessageRowKind::gap:
            break;
    }
    return text("");
}

Element renderMessagesTitle(const ChatInput& input)
{
    const auto& session = input.getSession();
    const auto& pane = input.getMessagePane();
    const auto title = " " + getRoomLabel(session.getRooms().getActiveRoomName())
                       + " · " + session.getStoreLabel() + " ";
    auto parts = Elements {
        renderPaneTitle(title, input.getFocusedPane() == ChatPane::messages),
        filler()};
    if (!pane.isFollowingNewestMessage())
    {
        const auto below = std::to_string(pane.countMessagesBelowCursor());
        parts.push_back(text(" " + below + " newer below · G ")
                        | color(accentColor));
    }
    return hbox(std::move(parts));
}

Element renderMessageRows(const ChatInput& input, const ChatLayout& layout)
{
    const auto& pane = input.getMessagePane();
    const auto& rows = pane.getRows();
    if (rows.empty())
        return text(" no messages yet") | dim;

    const auto& messages = input.getSession().getRooms().getActiveRoomMessages();
    const auto& reader = input.getSession().getIdentity().sender;
    const auto isFocused = input.getFocusedPane() == ChatPane::messages;
    const auto height = getPaneSizes(layout).messagesHeight;
    const auto end =
        std::min(static_cast<int>(rows.size()), pane.getTopRow() + height);
    auto lines = Elements {};
    for (auto index = pane.getTopRow(); index < end; ++index)
    {
        auto line = renderMessageRow(
            rows[static_cast<std::size_t>(index)], messages, reader);
        if (index == pane.getCursorRow())
            line = highlightCursorRow(line, isFocused);
        lines.push_back(line | size(WIDTH, LESS_THAN, layout.messagesWidth + 1));
    }
    return vbox(std::move(lines));
}

std::size_t findByteOffsetOfColumn(const std::string& line, int column)
{
    return static_cast<std::size_t>(vim::advance(line, 0, column, vim::size(line)));
}

Element renderEditorRowWithCursor(const std::string& row, const vim::Editor& editor)
{
    const auto cursorStart =
        findByteOffsetOfColumn(row, editor.cursorVisualColumn());
    const auto cursorEnd =
        findByteOffsetOfColumn(row, editor.cursorVisualColumn() + 1);
    const auto under = cursorEnd > cursorStart
                           ? row.substr(cursorStart, cursorEnd - cursorStart)
                           : " ";
    auto cursor = text(under) | inverted;
    if (editor.mode() == vim::Mode::insert)
        cursor = cursor | underlined;
    return hbox({text(row.substr(0, cursorStart)),
                 cursor | focus,
                 text(row.substr(cursorEnd))});
}

Element renderEditorFooter(const vim::Editor& editor)
{
    if (editor.promptChar() != 0)
        return text(std::string(1, editor.promptChar()) + editor.commandLine()
                    + "█");
    if (!editor.message().empty())
        return text(editor.message()) | color(Color::Red);
    if (editor.mode() == vim::Mode::insert)
        return text("-- INSERT --  Esc normal · Ctrl-S send") | dim;
    return text(":w send · :wq / :x send+close · :q! discard · i insert · Esc back")
           | dim;
}

Element renderEditorText(const vim::Editor& editor)
{
    const auto& lines = editor.lines();
    const auto cursorRow = editor.cursorVisualRow();
    const auto rows = editor.visualRows();
    auto body = Elements {};
    for (auto index = 0; index < static_cast<int>(rows.size()); ++index)
    {
        const auto& piece = rows[static_cast<std::size_t>(index)];
        const auto row = lines[static_cast<std::size_t>(piece.line)].substr(
            static_cast<std::size_t>(piece.start),
            static_cast<std::size_t>(piece.end - piece.start));
        const auto isCursorRow =
            index == cursorRow && editor.mode() != vim::Mode::command;
        body.push_back(isCursorRow ? renderEditorRowWithCursor(row, editor)
                                   : text(row));
    }
    return vbox(std::move(body));
}

Element renderComposeBox(ChatInput& input, const ChatLayout& layout)
{
    const auto& session = input.getSession();
    const auto& editor = input.getEditor();
    const auto title = " to " + getRoomLabel(session.getRooms().getActiveRoomName())
                       + " as " + session.getIdentity().sender + " ";
    return window(text(title) | bold,
                  vbox({renderEditorText(editor) | yframe | flex,
                        renderEditorFooter(editor)}))
           | color(accentColor) | size(HEIGHT, EQUAL, layout.composeBoxHeight);
}

Element renderMessagesPane(ChatInput& input, const ChatLayout& layout)
{
    auto parts = Elements {renderMessagesTitle(input),
                           renderMessageRows(input, layout) | flex};
    if (input.isComposing())
        parts.push_back(renderComposeBox(input, layout));
    return vbox(std::move(parts));
}

std::string getModeBadge(ChatInput& input)
{
    if (!input.isComposing())
    {
        if (input.getFocusedPane() == ChatPane::rooms)
            return " ROOMS ";
        return " MESSAGES ";
    }
    switch (input.getEditor().mode())
    {
        case vim::Mode::insert:
            return " INSERT ";
        case vim::Mode::command:
            return " COMMAND ";
        case vim::Mode::search:
            return " SEARCH ";
        case vim::Mode::normal:
            break;
    }
    return " NORMAL ";
}

std::size_t countUnreadMessages(const ChatRooms& rooms)
{
    const auto names = rooms.getRoomNames();
    return std::accumulate(names.begin(),
                           names.end(),
                           std::size_t {0},
                           [&](std::size_t total, const std::string& name)
                           { return total + rooms.getUnreadCount(name); });
}

bool isStatusFresh(const ChatStatus& status)
{
    return !status.text.empty()
           && getMillisecondsSinceEpoch() - status.shownAtMs < statusShownForMs;
}

Element renderStatusHint(ChatInput& input)
{
    if (input.isComposing())
        return text("Ctrl-S send · :w · Esc Esc back ") | bold;
    const auto pending = input.getPendingKeys();
    if (!pending.empty())
        return text(pending + " ") | bold;
    const auto& status = input.getStatus();
    if (isStatusFresh(status) && status.isError)
        return text(status.text + " ") | color(Color::Red);
    if (isStatusFresh(status))
        return text(status.text + " ");
    if (input.getFocusedPane() == ChatPane::rooms)
        return text("j k · l read · c write · :join · F1 help ") | dim;
    return text("j k ^d ^u G · h rooms · c write · F1 help ") | dim;
}

Element renderStatusLine(ChatInput& input)
{
    const auto& session = input.getSession();
    const auto& rooms = session.getRooms();
    auto parts =
        Elements {text(getModeBadge(input)) | bold | inverted,
                  text(" as ") | dim,
                  text(session.getIdentity().sender) | bold,
                  text("  │ ") | dim,
                  text(std::to_string(rooms.getRoomNames().size()) + " rooms")};
    if (const auto unread = countUnreadMessages(rooms); unread > 0)
        parts.push_back(text("  ● " + std::to_string(unread) + " unread")
                        | color(unreadColor));
    if (input.hasUnsentDraft() && !input.isComposing())
        parts.push_back(text("  ✎ draft") | color(accentColor));
    parts.push_back(filler());
    parts.push_back(text("  "));
    parts.push_back(renderStatusHint(input));
    return hbox(std::move(parts));
}

Element renderCommandLine(const LineEdit& line)
{
    const auto& typed = line.text();
    const auto at = line.cursor();
    const auto isCursorInside = at < typed.size();
    return hbox({text(":"),
                 text(typed.substr(0, at)),
                 text(isCursorInside ? typed.substr(at, 1) : " ") | inverted,
                 text(isCursorInside ? typed.substr(at + 1) : ""),
                 filler()});
}

Element renderHelpRow(const std::string& keyNames, const std::string& description)
{
    return hbox({text(keyNames) | bold | size(WIDTH, EQUAL, helpKeyColumnWidth),
                 text(description)});
}

Element renderHelp()
{
    return window(
               text(" Keys "),
               vbox({
                   renderHelpRow("j k  gg G", "move, top / bottom (counts: 5j)"),
                   renderHelpRow("Ctrl-d Ctrl-u", "half page down / up"),
                   renderHelpRow("h l  Tab", "rooms / messages / switch pane"),
                   renderHelpRow("Enter  l", "rooms: read the room"),
                   renderHelpRow("c  Enter  i", "write a message (i: insert mode)"),
                   renderHelpRow(":join #room", "open a room, creating it"),
                   renderHelpRow("q  :q  Ctrl-c", "quit"),
                   separator(),
                   text("Message box: vim. Esc normal, Esc again leaves it;") | dim,
                   text("the text stays for next time. Enter is a new line.") | dim,
                   text(":w sends, :wq :x Ctrl-S send and close, :q! drops it.")
                       | dim,
                   text("●N unread messages") | dim,
               }))
           | size(WIDTH, LESS_THAN, helpWidth + 1);
}
} // namespace

Element renderChat(ChatInput& input)
{
    const auto layout = calculateChatLayout(input);
    input.layOutPanes(getPaneSizes(layout));

    auto bottomLine = input.isCommandLineOpen()
                          ? renderCommandLine(input.getCommandLine())
                          : renderStatusLine(input);
    auto main = vbox({hbox({renderRoomsPane(input, layout)
                                | size(WIDTH, EQUAL, layout.roomsWidth),
                            separator(),
                            renderMessagesPane(input, layout) | flex})
                          | flex,
                      bottomLine});
    if (!input.isShowingHelp())
        return main;
    return dbox({main, renderHelp() | clear_under | center});
}
} // namespace iac
