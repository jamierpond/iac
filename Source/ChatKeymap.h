#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace iac
{
// What a key does in chat outside the compose box. The keys are DiffView's:
// the rooms list is dv's file tree, the messages its diff, the box its comment box.
enum class ChatAction
{
    down,
    up,
    top,
    bottom,
    halfPageDown,
    halfPageUp,
    focusRooms,
    focusMessages,
    togglePane,
    open,
    compose,
    composeInInsertMode,
    commandLine,
    help,
    quit,
    cancel
};

struct ChatCommand
{
    ChatAction action = ChatAction::cancel;
    int count = 1;
};

// The action's name in the usage log: "half-page-down" for ChatAction::halfPageDown.
std::string_view getChatActionName(ChatAction action);

// Turns key names ("j", "G", "C-d", "Tab", "Enter", "Esc") into commands, with
// vim counts ("5j") and two-key sequences ("gg"). Ported from DiffView's KeyParser.
class ChatKeyParser
{
public:
    // nullopt while a sequence is pending, or when it meant nothing.
    std::optional<ChatCommand> feed(const std::string& key);

    std::string getPendingKeys() const;

    void reset();

private:
    std::optional<ChatCommand> finishSingleKey(const std::string& key);
    std::optional<ChatCommand> finish(ChatAction action);

    int count = 0;
    std::string prefix;
};
} // namespace iac
