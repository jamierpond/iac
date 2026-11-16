#pragma once

#include <cstddef>
#include <string>

namespace iac
{
// A one-line text field with a cursor: chat's ':' command line. Ported from
// DiffView's telescope::LineEdit, which draws dv's ':' and '/' lines.
class LineEdit
{
public:
    // False for keys it doesn't use.
    bool feed(const std::string& key);

    void set(std::string text);
    const std::string& text() const;

    std::size_t cursor() const;

private:
    struct NamedKey;

    std::size_t getCursorIndex() const;
    void eraseFrom(int from);
    void deleteCharacterBehindCursor();
    void deleteCharacterUnderCursor();
    void moveLeft();
    void moveRight();
    void moveToStart();
    void moveToEnd();
    void deleteToStart();
    void deleteWordBehindCursor();

    std::string value;
    // A byte offset on a code point boundary, as text-buffer's helpers take it.
    int position = 0;
};
} // namespace iac
