#include "ChatKeymap.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace iac
{
namespace
{
constexpr auto largestCount = 99999;
constexpr auto decimalBase = 10;

const std::map<std::string, ChatAction>& getSingleKeys()
{
    static const auto keys = std::map<std::string, ChatAction> {
        {"j", ChatAction::down},
        {"Down", ChatAction::down},
        {"k", ChatAction::up},
        {"Up", ChatAction::up},
        {"G", ChatAction::bottom},
        {"End", ChatAction::bottom},
        {"Home", ChatAction::top},
        {"C-d", ChatAction::halfPageDown},
        {"PageDown", ChatAction::halfPageDown},
        {"C-u", ChatAction::halfPageUp},
        {"PageUp", ChatAction::halfPageUp},
        {"h", ChatAction::focusRooms},
        {"Left", ChatAction::focusRooms},
        {"l", ChatAction::focusMessages},
        {"Right", ChatAction::focusMessages},
        {"Tab", ChatAction::togglePane},
        {"Enter", ChatAction::open},
        {"c", ChatAction::compose},
        {"i", ChatAction::composeInInsertMode},
        {":", ChatAction::commandLine},
        {"F1", ChatAction::help},
        {"q", ChatAction::quit},
        {"Esc", ChatAction::cancel},
    };

    return keys;
}

const std::map<std::string, ChatAction>& getSequences()
{
    static const auto keys = std::map<std::string, ChatAction> {
        {"gg", ChatAction::top},
        {"g?", ChatAction::help},
    };

    return keys;
}

bool isDigitKey(const std::string& key)
{
    return key.size() == 1 && std::isdigit(static_cast<unsigned char>(key[0])) != 0;
}

// A leading 0 is not a count, as in vim.
bool isCountDigit(const std::string& key, int countSoFar)
{
    if (!isDigitKey(key))
        return false;
    return key[0] != '0' || countSoFar > 0;
}

struct SequenceMatch
{
    std::optional<ChatAction> action;
    bool startsSequence = false;
};

SequenceMatch findSequenceMatch(const std::string& typed)
{
    auto match = SequenceMatch {};

    for (const auto& [sequence, action]: getSequences())
    {
        if (sequence == typed)
            return {action, false};

        if (sequence.size() > typed.size() && sequence.starts_with(typed))
            match.startsSequence = true;
    }

    return match;
}
} // namespace

std::optional<ChatCommand> ChatKeyParser::feed(const std::string& key)
{
    if (key == "Esc")
    {
        reset();
        return ChatCommand {ChatAction::cancel, 1};
    }

    auto typed = prefix + key;
    const auto match = findSequenceMatch(typed);

    if (match.action)
        return finish(*match.action);

    if (match.startsSequence)
    {
        prefix = typed;
        return std::nullopt;
    }

    if (!prefix.empty())
    {
        reset();
        return std::nullopt;
    }

    if (isCountDigit(key, count))
    {
        count = std::min(count * decimalBase + (key[0] - '0'), largestCount);
        return std::nullopt;
    }

    return finishSingleKey(key);
}

std::optional<ChatCommand> ChatKeyParser::finishSingleKey(const std::string& key)
{
    auto single = getSingleKeys().find(key);

    if (single == getSingleKeys().end())
    {
        reset();
        return std::nullopt;
    }

    return finish(single->second);
}

std::optional<ChatCommand> ChatKeyParser::finish(ChatAction action)
{
    auto command = ChatCommand {action, count > 0 ? count : 1};
    reset();
    return command;
}

std::string ChatKeyParser::getPendingKeys() const
{
    if (count == 0)
        return prefix;

    return std::to_string(count) + prefix;
}

void ChatKeyParser::reset()
{
    count = 0;
    prefix.clear();
}

std::string_view getChatActionName(ChatAction action)
{
    static const auto names = std::map<ChatAction, std::string_view> {
        {ChatAction::down, "down"},
        {ChatAction::up, "up"},
        {ChatAction::top, "top"},
        {ChatAction::bottom, "bottom"},
        {ChatAction::halfPageDown, "half-page-down"},
        {ChatAction::halfPageUp, "half-page-up"},
        {ChatAction::focusRooms, "focus-rooms"},
        {ChatAction::focusMessages, "focus-messages"},
        {ChatAction::togglePane, "toggle-pane"},
        {ChatAction::open, "open"},
        {ChatAction::compose, "compose"},
        {ChatAction::composeInInsertMode, "compose-in-insert-mode"},
        {ChatAction::commandLine, "command-line"},
        {ChatAction::help, "help"},
        {ChatAction::quit, "quit"},
        {ChatAction::cancel, "cancel"},
    };

    return names.at(action);
}
} // namespace iac
