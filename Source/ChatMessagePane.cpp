#include "ChatMessagePane.h"

#include <EditorText.h>
#include <SplitText.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace iac
{
namespace
{
constexpr auto largestScrollMargin = 3;
constexpr auto screenFractionForScrollMargin = 4;

std::string copyBytes(const std::string& line, int start, int end)
{
    return line.substr(static_cast<std::size_t>(start),
                       static_cast<std::size_t>(end - start));
}

// After `start`, as dv's wrap never breaks at a piece's first byte.
std::optional<int> findLastSpaceBefore(const std::string& line, int start, int cut)
{
    for (auto at = cut - 1; at > start; --at)
    {
        if (line[static_cast<std::size_t>(at)] == ' ')
            return at;
    }
    return std::nullopt;
}

void addMessageRows(std::vector<ChatMessageRow>& rows,
                    const std::string& key,
                    const Message& message,
                    int wrapWidth)
{
    auto rowInMessage = 0;
    rows.push_back({ChatMessageRowKind::header, key, rowInMessage++, {}});
    for (const auto& line: text::splitText(message.text))
    {
        for (auto& piece: wrapLine(line, wrapWidth))
        {
            rows.push_back(
                {ChatMessageRowKind::text, key, rowInMessage++, std::move(piece)});
        }
    }
    rows.push_back({ChatMessageRowKind::gap, key, rowInMessage, {}});
}
} // namespace

std::vector<std::string> wrapLine(const std::string& line, int width)
{
    width = std::max(width, 1);
    const auto end = vim::size(line);
    auto pieces = std::vector<std::string> {};
    auto start = 0;

    while (vim::codepoints(line, start, end) > width)
    {
        const auto cut = vim::advance(line, start, width, end);
        const auto space = findLastSpaceBefore(line, start, cut);

        if (space)
        {
            pieces.push_back(copyBytes(line, start, *space));
            start = *space + 1;
        }
        else
        {
            pieces.push_back(copyBytes(line, start, cut));
            start = cut;
        }
    }

    pieces.push_back(copyBytes(line, start, end));
    return pieces;
}

std::vector<ChatMessageRow> buildChatMessageRows(const Messages& messages,
                                                 int wrapWidth)
{
    auto rows = std::vector<ChatMessageRow> {};
    for (const auto& [key, message]: messages)
        addMessageRows(rows, key, message, wrapWidth);
    if (!rows.empty())
        rows.pop_back();
    return rows;
}

int calculateTopRowShowingCursor(int cursor, int top, int height, int count)
{
    if (height <= 0)
        return 0;

    auto margin =
        std::min(largestScrollMargin, height / screenFractionForScrollMargin);

    if (cursor < top + margin)
        top = cursor - margin;

    if (cursor >= top + height - margin)
        top = cursor - height + margin + 1;

    return std::clamp(top, 0, std::max(0, count - height));
}

void ChatMessagePane::replaceRows(std::vector<ChatMessageRow> newRows)
{
    if (rows.empty() || isFollowingNewestMessage())
    {
        showRowsFromNewest(std::move(newRows));
        return;
    }

    const auto anchor = rows[static_cast<std::size_t>(cursorRow)];
    rows = std::move(newRows);
    cursorRow = findRowOfMessage(anchor.messageKey, anchor.rowInMessage);
}

void ChatMessagePane::showRowsFromNewest(std::vector<ChatMessageRow> newRows)
{
    rows = std::move(newRows);
    cursorRow = getLastRow();
}

int ChatMessagePane::findRowOfMessage(const std::string& messageKey,
                                      int rowInMessage) const
{
    auto found = -1;
    for (auto index = 0; index <= getLastRow(); ++index)
    {
        const auto& row = rows[static_cast<std::size_t>(index)];
        if (row.messageKey == messageKey && row.rowInMessage <= rowInMessage)
            found = index;
    }
    if (found < 0)
        return std::clamp(cursorRow, 0, getLastRow());
    return found;
}

int ChatMessagePane::getLastRow() const
{
    return std::max(0, static_cast<int>(rows.size()) - 1);
}

void ChatMessagePane::moveCursor(int delta)
{
    jumpToRow(cursorRow + delta);
}

void ChatMessagePane::jumpToRow(int row)
{
    cursorRow = std::clamp(row, 0, getLastRow());
}

void ChatMessagePane::scrollToShowCursor(int height)
{
    topRow = calculateTopRowShowingCursor(
        cursorRow, topRow, height, static_cast<int>(rows.size()));
}

const std::vector<ChatMessageRow>& ChatMessagePane::getRows() const
{
    return rows;
}

int ChatMessagePane::getCursorRow() const
{
    return cursorRow;
}

int ChatMessagePane::getTopRow() const
{
    return topRow;
}

bool ChatMessagePane::isFollowingNewestMessage() const
{
    return cursorRow >= getLastRow();
}

int ChatMessagePane::countMessagesBelowCursor() const
{
    if (rows.empty())
        return 0;
    const auto below = std::next(rows.begin(), cursorRow + 1);
    return static_cast<int>(
        std::count_if(below,
                      rows.end(),
                      [](const ChatMessageRow& row)
                      { return row.kind == ChatMessageRowKind::header; }));
}
} // namespace iac
