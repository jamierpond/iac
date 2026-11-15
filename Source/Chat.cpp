// iac chat — a full-screen view of a store: rooms down the side, the active
// room's messages in the middle, a prompt at the bottom. FTXUI owns the main
// loop; a poller thread watches the store's files and posts refreshes to it.

#include "Chat.h"
#include "Store.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace iac
{
namespace
{
using namespace ftxui;

using FileStamps =
    std::map<std::string,
             std::pair<std::filesystem::file_time_type, std::uintmax_t>>;

// Runs on the poller thread, so it touches the filesystem only — emberstore
// documents and UI state belong to the UI thread.
FileStamps stampStore(const std::filesystem::path& root)
{
    auto stamps = FileStamps {};
    auto error = std::error_code {};
    for (auto it = std::filesystem::directory_iterator {root, error};
         !error && it != std::filesystem::directory_iterator {};
         it.increment(error))
    {
        const auto file = it->path().filename().string();
        if (!file.ends_with(".json"))
            continue;
        auto statError = std::error_code {};
        stamps[file] = {it->last_write_time(statError), it->file_size(statError)};
    }
    return stamps;
}

// FNV-1a rather than std::hash, so an agent keeps its colour across runs and
// platforms.
Color senderColor(const std::string& name)
{
    static constexpr std::uint8_t palette[] = {
        33, 37, 71, 107, 133, 136, 166, 172, 99, 161, 30, 64, 169, 32, 208, 139};
    auto hash = std::uint32_t {2166136261u};
    for (const auto character: name)
    {
        hash ^= static_cast<unsigned char>(character);
        hash *= 16777619u;
    }
    return static_cast<Color::Palette256>(palette[hash % std::size(palette)]);
}

const auto accent = Color::RGB(215, 119, 87);

std::string roomLabel(const std::string& name)
{
    return name.empty() ? "(default)" : "#" + name;
}

class Chat
{
public:
    Chat(std::string senderToUse, std::string cwdToUse)
        : sender(std::move(senderToUse))
        , cwd(std::move(cwdToUse))
        , store(abbreviateHome(roomDirectory().str()))
    {
    }

    int run()
    {
        auto options = InputOption {};
        options.multiline = false;
        options.transform = [](InputState state) { return state.element; };
        options.on_enter = [this] { submit(); };
        auto prompt = Input(&input, options);

        auto root =
            Renderer(prompt, [this, prompt] { return render(prompt); })
            | CatchEvent([this](const Event& event) { return handleEvent(event); });

        refresh();

        auto running = std::atomic<bool> {true};
        auto poller = std::thread {
            [this, &running, root = std::filesystem::path {roomDirectory().str()}]
            {
                auto previous = stampStore(root);
                while (running)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds {200});
                    auto current = stampStore(root);
                    if (current == previous)
                        continue;
                    previous = std::move(current);
                    screen.Post([this] { refresh(); });
                    screen.PostEvent(Event::Custom);
                }
            }};

        screen.TrackMouse(false);
        screen.ForceHandleCtrlC(false);
        screen.Loop(root);

        running = false;
        poller.join();
        return 0;
    }

private:
    // roomDocumentName() names the active room's file; borrow it for the
    // other rooms so the naming rule stays in one place.
    emberstore::Document<Messages>& documentFor(const std::string& name)
    {
        if (const auto it = documents.find(name); it != documents.end())
            return it->second;

        const auto active = activeRoom.name;
        activeRoom.name = name;
        auto document = openRoom().document<Messages>(roomDocumentName());
        activeRoom.name = active;
        return documents.emplace(name, std::move(document)).first->second;
    }

    static std::string lastKey(const Messages& messages)
    {
        return messages.empty() ? std::string {} : messages.rbegin()->first;
    }

    void refresh()
    {
        auto names = std::vector<std::string> {""};
        if (const auto found = findRooms())
            names.insert(names.end(), found->names.begin(), found->names.end());
        if (!activeRoom.name.empty()
            && std::find(names.begin(), names.end(), activeRoom.name) == names.end())
        {
            names.push_back(activeRoom.name);
            std::sort(std::next(names.begin()), names.end());
        }
        rooms = std::move(names);

        for (const auto& name: rooms)
        {
            const auto& all = documentFor(name).peek();
            if (!started || name == activeRoom.name)
                lastSeen[name] = lastKey(all);
            else if (!lastSeen.contains(name))
                lastSeen[name] = {};
            unread[name] = static_cast<std::size_t>(
                std::distance(all.upper_bound(lastSeen[name]), all.end()));
        }
        started = true;
    }

    void selectRoom(const std::string& name)
    {
        lastSeen[activeRoom.name] = lastKey(documentFor(activeRoom.name).peek());
        activeRoom.name = name;
        follow = true;
        refresh();
    }

    void cycle(int step)
    {
        const auto current = std::find(rooms.begin(), rooms.end(), activeRoom.name);
        const auto size = static_cast<int>(rooms.size());
        const auto index = static_cast<int>(std::distance(rooms.begin(), current));
        selectRoom(rooms[static_cast<std::size_t>((index + step + size) % size)]);
    }

    void join(std::string name)
    {
        if (name.starts_with('#'))
            name.erase(0, 1);
        if (!validRoomName(name))
        {
            status = "usage: /join #name (a-z, 0-9, '-', '_')";
            return;
        }
        selectRoom(name);
    }

    void submit()
    {
        auto text = input;
        text.erase(0, text.find_first_not_of(' '));
        text.erase(text.find_last_not_of(' ') + 1);
        if (text.empty())
            return;

        input.clear();
        status.clear();
        if (text == "/quit")
        {
            screen.Exit();
            return;
        }
        if (text == "/join" || text.starts_with("/join "))
        {
            join(text.size() > 6 ? text.substr(6) : std::string {});
            return;
        }
        if (publish(text, sender, cwd) != 0)
            status = "could not write to " + store;
        follow = true;
        refresh();
    }

    // The message whose box spans the middle of the pane: the frame centres
    // the focused message, except when clamped at either end, so scrolling
    // steps from what is actually on screen.
    std::size_t centredMessage() const
    {
        const auto middle = (pane.y_min + pane.y_max) / 2;
        for (std::size_t i = 0; i < boxes.size(); ++i)
            if (boxes[i].y_max >= middle)
                return i;
        return boxes.empty() ? 0 : boxes.size() - 1;
    }

    void scrollBy(int step)
    {
        const auto count = documentFor(activeRoom.name).peek().size();
        if (count == 0)
            return;
        const auto target = static_cast<int>(centredMessage()) + step;
        if (target >= static_cast<int>(count) - 1)
        {
            follow = true;
            return;
        }
        follow = false;
        scroll = static_cast<std::size_t>(std::max(0, target));
    }

    bool handleEvent(const Event& event)
    {
        if (event == Event::CtrlC || event == Event::Escape)
        {
            screen.Exit();
            return true;
        }
        if (event == Event::Tab || event == Event::CtrlN)
        {
            cycle(1);
            return true;
        }
        if (event == Event::TabReverse || event == Event::CtrlP)
        {
            cycle(-1);
            return true;
        }
        if (event == Event::ArrowUp || event == Event::ArrowDown)
        {
            scrollBy(event == Event::ArrowUp ? -1 : 1);
            return true;
        }
        if (event == Event::PageUp || event == Event::PageDown)
        {
            scrollBy(event == Event::PageUp ? -10 : 10);
            return true;
        }
        if (event == Event::End)
        {
            follow = true;
            return true;
        }
        return false;
    }

    Element renderSidebar()
    {
        auto entries = Elements {text("rooms") | dim, text("")};
        auto width = 22;
        for (const auto& name: rooms)
        {
            const auto count = unread[name];
            const auto countWidth =
                count > 0 ? 1 + static_cast<int>(std::to_string(count).size()) : 0;
            width = std::max(width,
                             2 + static_cast<int>(roomLabel(name).size())
                                 + countWidth + 1);
        }
        width = std::min(width, 40);
        for (const auto& name: rooms)
        {
            const auto active = name == activeRoom.name;
            const auto count = unread[name];
            auto row = hbox({
                text(active ? "› " : "  ") | color(accent),
                text(roomLabel(name)),
                filler(),
                count > 0 ? text(" " + std::to_string(count)) | color(accent)
                          : text(""),
            });
            if (active)
                row = row | bold;
            else if (count == 0)
                row = row | dim;
            entries.push_back(row);
        }
        return vbox(std::move(entries)) | size(WIDTH, EQUAL, width);
    }

    Element renderMessage(const Message& message)
    {
        auto header = Elements {
            text(formatTime(message.timestamp)) | dim,
            text(" "),
            text(message.sender) | bold | color(senderColor(message.sender)),
        };
        if (!message.cwd.empty())
            header.push_back(text(" (" + abbreviateHome(message.cwd) + ")") | dim);
        return vbox({
            hbox(std::move(header)),
            hbox({text("  "), paragraph(message.text) | flex}),
        });
    }

    Element renderMessages()
    {
        const auto& all = documentFor(activeRoom.name).peek();
        if (all.empty())
            return vbox({filler(), text("no messages yet") | dim | center, filler()})
                   | flex;

        boxes.resize(all.size());
        const auto focused =
            follow ? all.size() - 1 : std::min(scroll, all.size() - 1);
        auto entries = Elements {};
        auto index = std::size_t {0};
        for (const auto& [key, message]: all)
        {
            auto entry =
                vbox({renderMessage(message), text("")}) | reflect(boxes[index]);
            if (index == focused)
                entry = entry | focus;
            entries.push_back(entry);
            ++index;
        }
        return vbox(std::move(entries)) | yframe | reflect(pane) | flex;
    }

    Element renderFooter()
    {
        auto hint = status.empty()
                        ? text("tab switch room · /join #name · ctrl-c quit") | dim
                        : text(status) | color(Color::Red);
        return hbox({text("  "),
                     hint,
                     filler(),
                     text(store + " · " + sender) | dim,
                     text("  ")});
    }

    Element render(const Component& prompt)
    {
        auto main = vbox({
            text(roomLabel(activeRoom.name)) | bold,
            text(""),
            renderMessages(),
        });
        return vbox({
            hbox({
                text(" "),
                renderSidebar(),
                separatorLight() | dim,
                text(" "),
                main | flex,
            }) | flex,
            hbox({text("> "), prompt->Render() | flex}) | borderRounded,
            renderFooter(),
        });
    }

    std::string sender;
    std::string cwd;
    std::string store;
    ScreenInteractive screen = ScreenInteractive::Fullscreen();

    std::string input;
    std::string status;
    std::vector<std::string> rooms;
    std::map<std::string, emberstore::Document<Messages>> documents;
    std::map<std::string, std::string> lastSeen;
    std::map<std::string, std::size_t> unread;
    bool started = false;

    bool follow = true;
    std::size_t scroll = 0;
    std::vector<Box> boxes;
    Box pane;
};
} // namespace

int runChat(const std::vector<std::string>& args)
{
    auto sender = defaultSender();
    auto cwd = currentDirectory();
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--from" && i + 1 < args.size())
            sender = args[++i];
        else if (args[i] == "--cwd" && i + 1 < args.size())
            cwd = args[++i];
    }
    return Chat {std::move(sender), std::move(cwd)}.run();
}
} // namespace iac
