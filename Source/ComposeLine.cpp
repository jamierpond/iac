#include "ComposeLine.h"

#include "Room.h"

#include <cctype>

namespace iac
{
namespace
{
constexpr auto blankCharacters = std::string_view {" \t\r\n"};
constexpr auto joinCommand = std::string_view {"/join"};
constexpr auto joinUsage = "usage: /join #name (a-z, 0-9, '-', '_')";

std::string trimBlanks(const std::string& text)
{
    const auto first = text.find_first_not_of(blankCharacters);
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(blankCharacters);
    return text.substr(first, last - first + 1);
}

ComposeAction parseJoinCommand(const std::string& text)
{
    auto name = trimBlanks(text.substr(joinCommand.size()));
    if (name.starts_with('#'))
        name.erase(0, 1);
    if (!isValidRoomName(name))
        return {ComposeActionKind::showError, joinUsage};
    return {ComposeActionKind::joinRoom, name};
}

bool isCommandWord(const std::string& text, std::string_view command)
{
    if (!text.starts_with(command))
        return false;
    return text.size() == command.size() || text[command.size()] == ' ';
}

ComposeAction parseSlashCommand(const std::string& text)
{
    if (text.starts_with("//"))
        return {ComposeActionKind::sendMessage, text.substr(1)};
    if (isCommandWord(text, joinCommand))
        return parseJoinCommand(text);
    if (text == "/rooms")
        return {ComposeActionKind::listRooms, {}};
    if (text == "/quit" || text == "/q")
        return {ComposeActionKind::quit, {}};
    return {ComposeActionKind::showError,
            "unknown command " + text + " (/join #name, /rooms, /quit; //text sends /text)"};
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

ComposeAction parseComposedText(const std::string& text)
{
    const auto trimmed = trimBlanks(text);
    if (trimmed.empty())
        return {};
    if (trimmed.starts_with('/'))
        return parseSlashCommand(trimmed);
    return {ComposeActionKind::sendMessage, trimmed};
}

std::vector<MentionRange> findMentions(std::string_view text)
{
    auto mentions = std::vector<MentionRange> {};
    for (auto at = text.find('@'); at != std::string_view::npos; at = text.find('@', at + 1))
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
