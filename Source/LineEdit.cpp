#include "LineEdit.h"

#include <EditorText.h>

#include <array>
#include <string_view>
#include <utility>

namespace iac
{
struct LineEdit::NamedKey
{
    std::string_view name;
    void (LineEdit::*run)();
};

bool LineEdit::feed(const std::string& key)
{
    static constexpr auto namedKeys = std::array<NamedKey, 10> {{
        {"Backspace", &LineEdit::deleteCharacterBehindCursor},
        {"Delete", &LineEdit::deleteCharacterUnderCursor},
        {"Left", &LineEdit::moveLeft},
        {"Right", &LineEdit::moveRight},
        {"Home", &LineEdit::moveToStart},
        {"C-a", &LineEdit::moveToStart},
        {"End", &LineEdit::moveToEnd},
        {"C-e", &LineEdit::moveToEnd},
        {"C-u", &LineEdit::deleteToStart},
        {"C-w", &LineEdit::deleteWordBehindCursor},
    }};

    for (const auto& namedKey: namedKeys)
    {
        if (namedKey.name == key)
        {
            (this->*namedKey.run)();
            return true;
        }
    }

    if (!vim::isPrintable(key))
        return false;

    value.insert(getCursorIndex(), key);
    position += vim::size(key);
    return true;
}

std::size_t LineEdit::getCursorIndex() const
{
    return static_cast<std::size_t>(position);
}

void LineEdit::eraseFrom(int from)
{
    const auto start = static_cast<std::size_t>(from);
    value.erase(start, getCursorIndex() - start);
    position = from;
}

void LineEdit::deleteCharacterBehindCursor()
{
    eraseFrom(vim::previousChar(value, position));
}

void LineEdit::deleteCharacterUnderCursor()
{
    const auto next = static_cast<std::size_t>(vim::nextChar(value, position));
    value.erase(getCursorIndex(), next - getCursorIndex());
}

void LineEdit::moveLeft()
{
    position = vim::previousChar(value, position);
}

void LineEdit::moveRight()
{
    position = vim::nextChar(value, position);
}

void LineEdit::moveToStart()
{
    position = 0;
}

void LineEdit::moveToEnd()
{
    position = vim::size(value);
}

void LineEdit::deleteToStart()
{
    eraseFrom(0);
}

void LineEdit::deleteWordBehindCursor()
{
    auto from = position;

    while (from > 0 && value[static_cast<std::size_t>(from - 1)] == ' ')
        --from;

    while (from > 0 && value[static_cast<std::size_t>(from - 1)] != ' ')
        --from;

    eraseFrom(from);
}

void LineEdit::set(std::string text)
{
    value = std::move(text);
    position = vim::size(value);
}

const std::string& LineEdit::text() const
{
    return value;
}

std::size_t LineEdit::cursor() const
{
    return getCursorIndex();
}
} // namespace iac
