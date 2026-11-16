#pragma once

#include "ChatInput.h"

#include <ftxui/dom/elements.hpp>

namespace iac
{
// The whole chat screen in DiffView's layout: the rooms list where dv has its
// file tree, the open room's messages where dv has the diff, the compose box
// under them while writing, and dv's status line. Sizes the panes to the
// terminal first, so the input's cursors stay on screen.
ftxui::Element renderChat(ChatInput& input);
} // namespace iac
