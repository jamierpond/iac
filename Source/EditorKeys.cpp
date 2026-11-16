#include "EditorKeys.h"

#include <utility>
#include <vector>

namespace iac
{
namespace
{
const std::vector<std::pair<ftxui::Event, std::string>>& getNamedEditorKeys()
{
    using ftxui::Event;
    static const auto keys = std::vector<std::pair<Event, std::string>> {
        {Event::Return, "Enter"},
        {Event::Escape, "Esc"},
        {Event::Backspace, "Backspace"},
        {Event::Delete, "Delete"},
        {Event::Tab, "Tab"},
        {Event::ArrowLeft, "Left"},
        {Event::ArrowRight, "Right"},
        {Event::ArrowUp, "Up"},
        {Event::ArrowDown, "Down"},
        {Event::Home, "Home"},
        {Event::End, "End"},
        {Event::CtrlS, "C-s"},
        {Event::CtrlV, "C-v"},
        {Event::CtrlC, "C-c"},
        {Event::CtrlR, "C-r"},
        {Event::CtrlW, "C-w"},
        {Event::CtrlU, "C-u"},
        {Event::CtrlH, "C-h"},
    };
    return keys;
}

const std::vector<std::pair<ftxui::Event, std::string>>& getNamedNavigationKeys()
{
    using ftxui::Event;
    static const auto keys = std::vector<std::pair<Event, std::string>> {
        {Event::Return, "Enter"},     {Event::Escape, "Esc"},
        {Event::Tab, "Tab"},          {Event::Backspace, "Backspace"},
        {Event::Delete, "Delete"},    {Event::ArrowDown, "Down"},
        {Event::ArrowUp, "Up"},       {Event::ArrowLeft, "Left"},
        {Event::ArrowRight, "Right"}, {Event::PageDown, "PageDown"},
        {Event::PageUp, "PageUp"},    {Event::Home, "Home"},
        {Event::End, "End"},          {Event::F1, "F1"},
        {Event::CtrlA, "C-a"},        {Event::CtrlC, "C-c"},
        {Event::CtrlD, "C-d"},        {Event::CtrlE, "C-e"},
        {Event::CtrlU, "C-u"},        {Event::CtrlW, "C-w"},
    };
    return keys;
}

std::string
    findKeyName(const std::vector<std::pair<ftxui::Event, std::string>>& named,
                const ftxui::Event& event)
{
    for (const auto& [namedEvent, name]: named)
    {
        if (event == namedEvent)
            return name;
    }
    if (event.is_character())
        return event.character();
    return {};
}
} // namespace

std::string getEditorKeyName(const ftxui::Event& event)
{
    return findKeyName(getNamedEditorKeys(), event);
}

std::string getNavigationKeyName(const ftxui::Event& event)
{
    return findKeyName(getNamedNavigationKeys(), event);
}
} // namespace iac
