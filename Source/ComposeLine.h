#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace iac
{
enum class ChatCommandKind
{
    nothing,
    joinRoom,
    showHelp,
    quit,
    showError
};

// What a line typed after ':' asks for. `argument` is the room name (no '#')
// for joinRoom, the explanation for showError.
struct ChatCommandLineAction
{
    ChatCommandKind kind = ChatCommandKind::nothing;
    std::string argument;
};

// "join #name", "help", and "q", "quit", "q!" or "quit!".
ChatCommandLineAction parseChatCommandLine(const std::string& line);

// The compose box's text as it is sent: without surrounding blank lines and
// spaces; "" when there is nothing to send.
std::string trimComposedMessage(const std::string& text);

struct MentionRange
{
    std::size_t start = 0;
    std::size_t length = 0;
};

// Each "@role" in the text: an '@' not preceded by a letter or digit (so
// emails don't count), followed by letters, digits, '-', '_' or '.', minus a trailing '.'.
std::vector<MentionRange> findMentions(std::string_view text);
} // namespace iac
