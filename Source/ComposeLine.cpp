#include "ComposeLine.h"

#include "Room.h"

#include <cctype>
#include <set>

namespace iac
{
namespace
{
constexpr auto blankCharacters = std::string_view {" \t\r\n"};
constexpr auto joinCommand = std::string_view {"join"};
constexpr auto joinUsage = "usage: :join #name (a-z, 0-9, '-', '_')";

std::string trimBlanks(const std::string& text)
{
    const auto first = text.find_first_not_of(blankCharacters);
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(blankCharacters);
    return text.substr(first, last - first + 1);
}

ChatCommandLineAction parseJoinCommand(const std::string& text)
{
    auto name = trimBlanks(text.substr(joinCommand.size()));
    if (name.starts_with('#'))
        name.erase(0, 1);
    if (!isValidRoomName(name))
        return {ChatCommandKind::showError, joinUsage};
    return {ChatCommandKind::joinRoom, name};
}

bool isCommandWord(const std::string& text, std::string_view command)
{
    if (!text.starts_with(command))
        return false;
    return text.size() == command.size() || text[command.size()] == ' ';
}

bool isQuitCommand(const std::string& text)
{
    static const auto quitCommands =
        std::set<std::string> {"q", "quit", "q!", "quit!"};
    return quitCommands.contains(text);
}

bool isWordCharacter(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0;
}

bool isMentionCharacter(char character)
{
    return isWordCharacter(character) || character == '-' || character == '_'
           || character == '.';
}

std::size_t findMentionLength(std::string_view text, std::size_t at)
{
    auto end = at + 1;
    while (end < text.size() && isMentionCharacter(text[end]))
        ++end;
    while (end > at + 1 && text[end - 1] == '.')
        --end;
    return end - at;
}
} // namespace

ChatCommandLineAction parseChatCommandLine(const std::string& line)
{
    const auto trimmed = trimBlanks(line);
    if (trimmed.empty())
        return {};
    if (isCommandWord(trimmed, joinCommand))
        return parseJoinCommand(trimmed);
    if (trimmed == "help")
        return {ChatCommandKind::showHelp, {}};
    if (isQuitCommand(trimmed))
        return {ChatCommandKind::quit, {}};
    return {ChatCommandKind::showError,
            "not a command: " + trimmed + " (:join #name, :help, :q)"};
}

std::string trimComposedMessage(const std::string& text)
{
    return trimBlanks(text);
}

std::vector<MentionRange> findMentions(std::string_view text)
{
    auto mentions = std::vector<MentionRange> {};
    for (auto at = text.find('@'); at != std::string_view::npos;
         at = text.find('@', at + 1))
    {
        if (at > 0 && isWordCharacter(text[at - 1]))
            continue;
        const auto length = findMentionLength(text, at);
        if (length > 1)
            mentions.push_back({at, length});
    }
    return mentions;
}
} // namespace iac
