#pragma once

#include "ChatSession.h"
#include "ChatView.h"

#include <VimEditor.h>

#include <emberstore/FileWatcher.h>

#include <eacp/Core/Threads/Timer.h>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

    bool onEvent(const ftxui::Event& event);
    bool onScreenKey(const ftxui::Event& event);
    std::vector<std::pair<ftxui::Event, std::function<void()>>> makeScreenKeys();
    void selectNeighbouringRoom(int step);
    void scrollByPage(bool isTowardsOlder);
    void feedEditor(const std::string& key);
    void sendPrompt();
    void quit(std::string_view via);

    ChatSession session;
    vim::Editor editor;
    ChatStatus status;
    std::vector<std::pair<ftxui::Event, std::function<void()>>> screenKeys = makeScreenKeys();
    std::map<std::string, std::unique_ptr<emberstore::FileWatcher>> roomWatchers;
    int ticksSinceRoomScan = 0;
    int sentCount = 0;

    ftxui::App screen = ftxui::App::Fullscreen();
    ftxui::Component root;
    std::unique_ptr<ftxui::Loop> loop;
    eacp::Threads::Timer pump;
};

// Runs chat until the person quits; the process's exit code.
int runChatScreen(const Room& storeAndFirstRoom, const ChatIdentity& identity);
} // namespace iac
