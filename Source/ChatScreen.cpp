#include "ChatScreen.h"

#include "ChatView.h"
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
} // namespace

ChatScreen::ChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity)
    : session(storeAndFirstRoom, identity)
    , pump([this] { tick(); }, eacp::Time::MS {pumpIntervalMs})
{
    eacp::Apps::setDockIconVisible(false);

    root = ftxui::Renderer([this] { return renderChat(input); })
           | ftxui::CatchEvent([this](const ftxui::Event& event)
                               { return onEvent(event); });
    screen.TrackMouse(false);
    // Ctrl-C is a key here, so quitting with it is logged.
    screen.ForceHandleCtrlC(false);
    loop = std::make_unique<ftxui::Loop>(&screen, root);

    watchNewRooms();
    logEvent(
        "chat.open",
        {{"room", getRoomLabel(storeAndFirstRoom.name)},
         {"rooms", static_cast<int>(session.getRooms().getRoomNames().size())}});
}

ChatScreen::~ChatScreen()
{
    logEvent("chat.close", {{"sent", input.getSentCount()}});
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
    addWatchersForNewRooms();
    redraw();
}

void ChatScreen::addWatchersForNewRooms()
{
    for (const auto& name: session.getRooms().getRoomNames())
    {
        if (roomWatchers.contains(name))
            continue;
        const auto room = session.getRoom(name);
        const auto path =
            getRoomDirectory(room)
            / (getRoomDocumentName(room) + std::string {documentFileExtension});
        roomWatchers[name] = std::make_unique<emberstore::FileWatcher>(
            path, [this, name] { onRoomFileChanged(name); });
    }
}

void ChatScreen::onRoomFileChanged(const std::string& name)
{
    input.reloadRoom(name);
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
    if (event == ftxui::Event::CtrlC)
    {
        logEvent("chat.quit", {{"via", "ctrl-c"}});
        screen.Exit();
        return true;
    }

    if (input.isComposing())
        input.handleKey(getEditorKeyName(event));
    else
        input.handleKey(getNavigationKeyName(event));

    addWatchersForNewRooms();
    if (input.hasQuit())
        screen.Exit();
    return true;
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
