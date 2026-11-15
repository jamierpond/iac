#include "ChatScreen.h"

#include "EditorKeys.h"
#include "UsageLog.h"

#include <eacp/Core/App/App.h>

#include <optional>

namespace iac
{
namespace
{
constexpr auto pumpIntervalMs = 16;
// New room files are looked for about once a second, on the pump's ticks.
constexpr auto ticksBetweenRoomScans = 60;
constexpr auto messagesPerPage = std::size_t {5};

std::string_view getEditorModeName(vim::Mode mode)
{
    switch (mode)
    {
        case vim::Mode::normal:
            return "normal";
        case vim::Mode::insert:
            return "insert";
        case vim::Mode::command:
            return "command";
        case vim::Mode::search:
            return "search";
    }
    return "unknown";
}
} // namespace

ChatScreen::ChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity)
    : session(storeAndFirstRoom, identity)
    , pump([this] { tick(); }, eacp::Time::MS {pumpIntervalMs})
{
    eacp::Apps::setDockIconVisible(false);
    editor.feed("i");

    root = ftxui::Renderer([this] { return renderChat(session, editor, status); })
           | ftxui::CatchEvent([this](const ftxui::Event& event) { return onEvent(event); });
    screen.TrackMouse(false);
    // Ctrl-C is a key here, so quitting goes through quit() and is logged.
    screen.ForceHandleCtrlC(false);
    loop = std::make_unique<ftxui::Loop>(&screen, root);

    watchNewRooms();
    logEvent("chat.open",
             {{"room", getRoomLabel(storeAndFirstRoom.name)},
              {"rooms", static_cast<int>(session.getRooms().getRoomNames().size())}});
}

ChatScreen::~ChatScreen()
{
    logEvent("chat.close", {{"sent", sentCount}});
}

void ChatScreen::tick()
{
    if (loop->HasQuitted())
    {
        eacp::Apps::quit();
        return;
    }
    if (++ticksSinceRoomScan >= ticksBetweenRoomScans)
    {
        ticksSinceRoomScan = 0;
        session.reloadRoomList();
        watchNewRooms();
    }
    loop->RunOnce();
}

void ChatScreen::watchNewRooms()
{
    for (const auto& name: session.getRooms().getRoomNames())
    {
        if (roomWatchers.contains(name))
            continue;
        const auto room = session.getRoom(name);
        const auto path = getRoomDirectory(room) / (getRoomDocumentName(room) + std::string {documentFileExtension});
        roomWatchers[name] = std::make_unique<emberstore::FileWatcher>(
            path, [this, name] { onRoomFileChanged(name); });
    }
    redraw();
}

void ChatScreen::onRoomFileChanged(const std::string& name)
{
    session.reloadRoom(name);
    redraw();
}

void ChatScreen::redraw()
{
    screen.PostEvent(ftxui::Event::Custom);
}

bool ChatScreen::onEvent(const ftxui::Event& event)
{
    if (event == ftxui::Event::Custom)
        return false;
    if (onScreenKey(event))
        return true;

    const auto key = getEditorKeyName(event);
    if (!key.empty())
        feedEditor(key);
    return true;
}

// Keys that belong to the screen rather than the prompt. Enter sends unless
// the editor's ':' or '/' line is open, where it runs that line.
bool ChatScreen::onScreenKey(const ftxui::Event& event)
{
    if (event == ftxui::Event::Return && editor.promptChar() != 0)
        return false;
    for (const auto& [key, action]: screenKeys)
    {
        if (event == key)
        {
            action();
            return true;
        }
    }
    return false;
}

std::vector<std::pair<ftxui::Event, std::function<void()>>> ChatScreen::makeScreenKeys()
{
    using ftxui::Event;
    return {
        {Event::CtrlC, [this] { quit("ctrl-c"); }},
        {Event::Return, [this] { sendPrompt(); }},
        {Event::Tab, [this] { selectNeighbouringRoom(1); }},
        {Event::CtrlN, [this] { selectNeighbouringRoom(1); }},
        {Event::TabReverse, [this] { selectNeighbouringRoom(-1); }},
        {Event::CtrlP, [this] { selectNeighbouringRoom(-1); }},
        {Event::PageUp, [this] { scrollByPage(true); }},
        {Event::PageDown, [this] { scrollByPage(false); }},
    };
}

void ChatScreen::selectNeighbouringRoom(int step)
{
    auto& rooms = session.getRooms();
    rooms.selectNeighbouringRoom(step);
    status = {};
    logEvent("chat.room", {{"room", getRoomLabel(rooms.getActiveRoomName())}, {"via", "cycle"}});
}

void ChatScreen::scrollByPage(bool isTowardsOlder)
{
    auto& rooms = session.getRooms();
    if (isTowardsOlder)
        rooms.scrollToOlderMessages(messagesPerPage);
    else
        rooms.scrollToNewerMessages(messagesPerPage);
    logEvent("chat.scroll",
             {{"direction", isTowardsOlder ? "older" : "newer"},
              {"below", static_cast<int>(rooms.getMessagesBelowView())}});
}

void ChatScreen::feedEditor(const std::string& key)
{
    const auto modeBefore = editor.mode();
    const auto outcome = editor.feed(key);
    if (editor.mode() != modeBefore)
    {
        logEvent("chat.editor_mode",
                 {{"from", getEditorModeName(modeBefore)}, {"to", getEditorModeName(editor.mode())}});
    }
    switch (outcome)
    {
        case vim::Outcome::save:
        case vim::Outcome::saveAndClose:
            sendPrompt();
            break;
        case vim::Outcome::close:
            quit(":q");
            break;
        case vim::Outcome::discard:
            editor.setText({});
            logEvent("chat.discard");
            break;
        case vim::Outcome::none:
        case vim::Outcome::paste:
            break;
    }
}

void ChatScreen::sendPrompt()
{
    const auto result = session.submitComposedText(editor.text());
    status = {result.status, result.isError};
    if (result.kind == ComposeActionKind::quit)
    {
        quit("/quit");
        return;
    }
    if (result.isError)
    {
        logEvent("chat.error", {{"reason", result.status}});
        return;
    }
    if (result.kind == ComposeActionKind::nothing)
        return;

    editor.setText({});
    if (editor.mode() != vim::Mode::insert)
        editor.feed("i");
    if (result.kind == ComposeActionKind::sendMessage)
        ++sentCount;
    if (result.kind == ComposeActionKind::joinRoom)
        watchNewRooms();
    logEvent("chat.submit",
             {{"kind", result.kind == ComposeActionKind::sendMessage ? "message" : "command"},
              {"room", getRoomLabel(session.getRooms().getActiveRoomName())}});
}

void ChatScreen::quit(std::string_view via)
{
    logEvent("chat.quit", {{"via", via}});
    screen.Exit();
}

// Not Apps::run<ChatScreen>: on Linux it mistakes an executable started
// through PATH for a plugin and returns before the loop runs.
int runChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity)
{
    auto chat = std::optional<ChatScreen> {};
    eacp::Threads::runEventLoop([&] { chat.emplace(storeAndFirstRoom, identity); });
    chat.reset();
    return eacp::Apps::getReturnValue();
}
} // namespace iac
