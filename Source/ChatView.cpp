#include "ChatView.h"

#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>

namespace iac
{
namespace
{
using namespace ftxui;
using ftxui::text;

constexpr auto sidebarMinimumWidth = 18;
constexpr auto sidebarMaximumWidth = 32;
constexpr auto sidebarPaddingWidth = 6;
constexpr auto messagesDrawnAboveView = std::size_t {200};
constexpr auto promptMinimumTextRows = 1;
constexpr auto promptMaximumTextRows = 8;
constexpr auto promptBorderWidth = 2;
constexpr auto fnvOffsetBasis = std::uint32_t {2166136261u};
constexpr auto fnvPrime = std::uint32_t {16777619u};
constexpr auto utf8ContinuationMask = 0xC0;
constexpr auto utf8ContinuationBits = 0x80;

// 256-colour palette entries that read on dark and light terminals alike.
constexpr auto senderPalette = std::array<std::uint8_t, 16> {
    33, 37, 71, 107, 133, 136, 166, 172, 99, 161, 30, 64, 169, 32, 208, 139};

const auto accentColor = Color::Yellow;

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

Element renderSidebarRow(const ChatRooms& rooms, const std::string& name)
{
    const auto isActive = name == rooms.getActiveRoomName();
    const auto unread = rooms.getUnreadCount(name);
    auto count = text("");
    if (unread > 0)
        count = text(" " + std::to_string(unread)) | color(accentColor) | bold;
    auto row = hbox({text(isActive ? "› " : "  ") | color(accentColor),
                     text(getRoomLabel(name)),
                     filler(),
                     count});
    if (isActive)
        return row | bold;
    if (unread == 0)
        return row | dim;
    return row;
}

int getSidebarWidth(const ChatRooms& rooms)
{
    auto width = sidebarMinimumWidth;
    for (const auto& name: rooms.getRoomNames())
    {
        const auto labelWidth = static_cast<int>(getRoomLabel(name).size());
        width = std::max(width, labelWidth + sidebarPaddingWidth);
    }
    return std::min(width, sidebarMaximumWidth);
}

Element renderSidebar(const ChatRooms& rooms)
{
    auto rows = Elements {text(" rooms") | dim, text("")};
    for (const auto& name: rooms.getRoomNames())
        rows.push_back(renderSidebarRow(rooms, name));
    return vbox(std::move(rows)) | size(WIDTH, EQUAL, getSidebarWidth(rooms));
}

bool isMentionOf(std::string_view mention, const std::string& sender)
{
    return mention.substr(1) == sender;
}

// Builds a line of words for hflow, so it wraps between words; styled pieces
// such as mentions join the word they touch.
class WrappingLineBuilder
{
public:
    void addPlainText(std::string_view plain)
    {
        for (const auto character: plain)
        {
            if (character != ' ')
            {
                plainRun += character;
                continue;
            }
            finishWord();
            words.push_back(text(" "));
        }
    }

    void addStyledPiece(Element piece)
    {
        finishPlainRun();
        pieces.push_back(std::move(piece));
    }

    Element build()
    {
        finishWord();
        return hflow(std::move(words));
    }

private:
    void finishPlainRun()
    {
        if (!plainRun.empty())
            pieces.push_back(text(plainRun));
        plainRun.clear();
    }

    void finishWord()
    {
        finishPlainRun();
        if (!pieces.empty())
            words.push_back(hbox(std::move(pieces)));
        pieces = {};
    }

    Elements words;
    Elements pieces;
    std::string plainRun;
};

// One line of a message with each "@role" coloured, and a mention of the
// person chatting shown inverted.
Element renderMessageLine(const std::string& line, const std::string& reader)
{
    auto builder = WrappingLineBuilder {};
    auto position = std::size_t {0};
    for (const auto& mention: findMentions(line))
    {
        builder.addPlainText(std::string_view {line}.substr(position, mention.start - position));
        const auto mentionText = line.substr(mention.start, mention.length);
        auto shown = text(mentionText) | bold | color(accentColor);
        if (isMentionOf(mentionText, reader))
            shown = shown | inverted;
        builder.addStyledPiece(shown);
        position = mention.start + mention.length;
    }
    builder.addPlainText(std::string_view {line}.substr(position));
    return builder.build();
}

std::vector<std::string> splitLines(const std::string& text)
{
    auto lines = std::vector<std::string> {};
    auto start = std::size_t {0};
    for (auto end = text.find('\n'); end != std::string::npos; end = text.find('\n', start))
    {
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    lines.push_back(text.substr(start));
    return lines;
}

Element renderMessage(const Message& message, const std::string& reader)
{
    auto header = Elements {text(formatTimeOfDay(message)) | dim,
                            text(" "),
                            text(message.sender) | bold | color(getSenderColor(message.sender))};
    if (!message.cwd.empty())
        header.push_back(text(" (" + abbreviateHome(message.cwd) + ")") | dim);

    auto body = Elements {hbox(std::move(header))};
    for (const auto& line: splitLines(message.text))
        body.push_back(hbox({text("  "), renderMessageLine(line, reader) | flex}));
    body.push_back(text(""));
    return vbox(std::move(body));
}

Element renderMessages(const ChatRooms& rooms, const std::string& reader)
{
    const auto& messages = rooms.getActiveRoomMessages();
    if (messages.empty())
        return vbox({filler(), text("no messages yet") | dim | center, filler()}) | flex;

    const auto below = std::min(rooms.getMessagesBelowView(), messages.size() - 1);
    const auto end = messages.size() - below;
    const auto start = end > messagesDrawnAboveView ? end - messagesDrawnAboveView : 0;

    // A filler on top keeps a short room's messages down by the prompt.
    auto entries = Elements {filler()};
    auto first = std::next(messages.begin(), static_cast<std::ptrdiff_t>(start));
    auto last = std::next(messages.begin(), static_cast<std::ptrdiff_t>(end));
    for (auto it = first; it != last; ++it)
        entries.push_back(renderMessage(it->second, reader));
    entries.back() = entries.back() | focus;
    return vbox(std::move(entries)) | yframe | flex;
}

Element renderRoomTitle(const ChatSession& session)
{
    const auto& rooms = session.getRooms();
    auto title = Elements {text(getRoomLabel(rooms.getActiveRoomName())) | bold, filler()};
    if (rooms.getMessagesBelowView() > 0)
    {
        const auto newer = std::to_string(rooms.getMessagesBelowView());
        title.push_back(text(newer + " newer below · PgDn ") | color(accentColor));
    }
    title.push_back(text(session.getStoreLabel() + " ") | dim);
    return hbox(std::move(title));
}

std::size_t findByteOffsetOfColumn(const std::string& line, int column)
{
    auto bytes = std::size_t {0};
    for (auto counted = 0; counted < column && bytes < line.size(); ++counted)
    {
        ++bytes;
        while (bytes < line.size()
               && (static_cast<unsigned char>(line[bytes]) & utf8ContinuationMask)
                      == utf8ContinuationBits)
            ++bytes;
    }
    return bytes;
}

Element renderEditorRowWithCursor(const std::string& row, const vim::Editor& editor)
{
    const auto cursorStart = findByteOffsetOfColumn(row, editor.cursorVisualColumn());
    const auto cursorEnd = findByteOffsetOfColumn(row, editor.cursorVisualColumn() + 1);
    const auto under = cursorEnd > cursorStart ? row.substr(cursorStart, cursorEnd - cursorStart) : " ";
    auto cursor = text(under) | inverted;
    if (editor.mode() == vim::Mode::insert)
        cursor = cursor | underlined;
    return hbox({text(row.substr(0, cursorStart)), cursor | focus, text(row.substr(cursorEnd))});
}

Element renderEditorFooter(const vim::Editor& editor)
{
    if (editor.promptChar() != 0)
        return text(std::string(1, editor.promptChar()) + editor.commandLine() + "█");
    if (!editor.message().empty())
        return text(editor.message()) | color(Color::Red);
    if (editor.mode() == vim::Mode::insert)
        return text("-- INSERT --  Enter send · Esc normal") | dim;
    return text("Enter send · i insert · :q quit") | dim;
}

Element renderPrompt(const ChatSession& session, vim::Editor& editor)
{
    const auto terminalWidth = Terminal::Size().dimx;
    editor.setWrapWidth(std::max(1, terminalWidth - promptBorderWidth));

    const auto& lines = editor.lines();
    const auto cursorRow = editor.cursorVisualRow();
    auto body = Elements {};
    const auto rows = editor.visualRows();
    for (auto index = 0; index < static_cast<int>(rows.size()); ++index)
    {
        const auto& piece = rows[static_cast<std::size_t>(index)];
        const auto row = lines[static_cast<std::size_t>(piece.line)].substr(
            static_cast<std::size_t>(piece.start), static_cast<std::size_t>(piece.end - piece.start));
        const auto isCursorRow = index == cursorRow && editor.mode() != vim::Mode::command;
        body.push_back(isCursorRow ? renderEditorRowWithCursor(row, editor) : text(row));
    }
    const auto textRows = std::clamp(static_cast<int>(rows.size()), promptMinimumTextRows, promptMaximumTextRows);

    const auto& rooms = session.getRooms();
    const auto title = " to " + getRoomLabel(rooms.getActiveRoomName()) + " as "
                       + session.getIdentity().sender + " ";
    return window(text(title) | bold,
                  vbox({vbox(std::move(body)) | yframe | size(HEIGHT, EQUAL, textRows),
                        renderEditorFooter(editor)}))
           | color(accentColor);
}

Element renderStatus(const ChatStatus& status)
{
    if (status.isError)
        return text(" " + status.text) | color(Color::Red);
    if (!status.text.empty())
        return text(" " + status.text);
    return text(" Tab/C-n next room · S-Tab/C-p previous · PgUp/PgDn scroll · "
                "/join #room · /rooms · C-c quit")
           | dim;
}
} // namespace

ftxui::Element renderChat(const ChatSession& session,
                          vim::Editor& editor,
                          const ChatStatus& status)
{
    const auto& rooms = session.getRooms();
    const auto& reader = session.getIdentity().sender;
    auto room = vbox({renderRoomTitle(session), text(""), renderMessages(rooms, reader)});
    return vbox({hbox({renderSidebar(rooms), separatorLight() | dim, text(" "), room | flex}) | flex,
                 renderPrompt(session, editor),
                 renderStatus(status)});
}
} // namespace iac
