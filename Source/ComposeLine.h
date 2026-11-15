#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace iac
{
enum class ComposeActionKind
{
    nothing,
    sendMessage,
    joinRoom,
    listRooms,
    quit,
    showError
};

// What the text typed into chat's prompt asks for. `argument` is the message
// for sendMessage, the room name (no '#') for joinRoom, the explanation for showError.
struct ComposeAction
{
    ComposeActionKind kind = ComposeActionKind::nothing;
    std::string argument;
};

// "/join #name", "/rooms" and "/quit" are commands; anything else is a message.
// Surrounding blank lines and spaces are dropped; "//text" sends "/text".
ComposeAction parseComposedText(const std::string& text);

struct MentionRange
{
    std::size_t start = 0;
    std::size_t length = 0;
};

// Each "@role" in the text: an '@' not preceded by a letter or digit (so
// emails don't count), followed by letters, digits, '-', '_' or '.', minus a trailing '.'.
std::vector<MentionRange> findMentions(std::string_view text);
} // namespace iac
