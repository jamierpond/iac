#pragma once

#include <ftxui/component/event.hpp>

#include <string>

namespace iac
{
// The vim::Editor key name for a terminal event ("Enter", "Esc", "C-w", or the
// typed character), empty for keys the editor has no use for. Copied from
// DiffView's Source/Comments.cpp: a candidate for a shared terminal-vim library.
std::string getEditorKeyName(const ftxui::Event& event);
} // namespace iac
