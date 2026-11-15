#pragma once

#include "ChatSession.h"

#include <VimEditor.h>

#include <ftxui/dom/elements.hpp>

#include <string>

namespace iac
{
// The line under the prompt: what the last command said, or the key hints.
struct ChatStatus
{
    std::string text;
    bool isError = false;
};

// The whole chat screen: rooms down the left with unread counts, the open
// room's messages, the vim prompt and the status line.
ftxui::Element renderChat(const ChatSession& session,
                          vim::Editor& editor,
                          const ChatStatus& status);
} // namespace iac
