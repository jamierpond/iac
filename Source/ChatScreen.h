#pragma once

#include "ChatInput.h"
#include "ChatSession.h"

#include <emberstore/FileWatcher.h>

#include <eacp/Core/Threads/Timer.h>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>

#include <map>
#include <memory>
#include <string>

namespace iac
{
// iac chat's full-screen terminal UI over a local store. eacp owns the main
// thread and a timer pumps FTXUI's loop, as in DiffView; room file watchers
// land on the same thread, so nothing here is locked.
class ChatScreen
{
public:
    ChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity);
    ~ChatScreen();

private:
    void tick();
    void watchNewRooms();
    void onRoomFileChanged(const std::string& name);
    void redraw();

    void addWatchersForNewRooms();
    bool onEvent(const ftxui::Event& event);

    ChatSession session;
    ChatInput input {session};
    std::map<std::string, std::unique_ptr<emberstore::FileWatcher>> roomWatchers;
    int ticksSinceRoomScan = 0;

    ftxui::App screen = ftxui::App::Fullscreen();
    ftxui::Component root;
    std::unique_ptr<ftxui::Loop> loop;
    eacp::Threads::Timer pump;
};

// Runs chat until the person quits; the process's exit code.
int runChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity);
} // namespace iac
