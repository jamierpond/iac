#pragma once

#include "Message.h"

#include <string>
#include <vector>

namespace iac
{
enum class ChatMessageRowKind
{
    header,
    text,
    gap
};

// One screen row of the open room: a message's header (time, sender), one
// wrapped piece of its text, or the blank row after it.
struct ChatMessageRow
{
    ChatMessageRowKind kind = ChatMessageRowKind::text;
    std::string messageKey;
    int rowInMessage = 0;
    std::string text;
};

// Every message as rows, its text wrapped at `wrapWidth` columns.
std::vector<ChatMessageRow> buildChatMessageRows(const Messages& messages,
                                                 int wrapWidth);

// Splits a line into pieces of at most `width` code points, at the last space
// that fits where there is one. Never empty: "" wraps to {""}. DiffView's wrap.
std::vector<std::string> wrapLine(const std::string& line, int width);

// The first row to draw so the cursor row stays on screen with a few rows of
// margin, scrolling as little as possible. DiffView's scrollFor.
int calculateTopRowShowingCursor(int cursor, int top, int height, int count);

// The open room's rows and the cursor that j/k move through them, as dv's
// diff cursor. The cursor on the last row follows new messages.
class ChatMessagePane
{
public:
    // Keeps the cursor on the same row of the same message, or on the last
    // row when it was following the newest message.
    void replaceRows(std::vector<ChatMessageRow> newRows);

    // Rows of another room: the cursor starts on the newest message.
    void showRowsFromNewest(std::vector<ChatMessageRow> newRows);

    void moveCursor(int delta);
    void jumpToRow(int row);
    void scrollToShowCursor(int height);

    const std::vector<ChatMessageRow>& getRows() const;
    int getCursorRow() const;
    int getTopRow() const;
    bool isFollowingNewestMessage() const;
    int countMessagesBelowCursor() const;

private:
    int findRowOfMessage(const std::string& messageKey, int rowInMessage) const;
    int getLastRow() const;

    std::vector<ChatMessageRow> rows;
    int cursorRow = 0;
    int topRow = 0;
};
} // namespace iac
