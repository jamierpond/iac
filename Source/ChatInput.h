#pragma once

#include "ChatKeymap.h"
#include "ChatMessagePane.h"
#include "ChatSession.h"
#include "LineEdit.h"

#include <VimEditor.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace iac
{
// The two panes, as DiffView's tree and diff: the rooms list and the open room.
enum class ChatPane
{
    rooms,
    messages
};

// What the status line says for a while: a command's result or an error.
struct ChatStatus
{
    std::string text;
    bool isError = false;
    std::int64_t shownAtMs = 0;
};

// The rows each pane has on screen this frame, and the width messages wrap at.
struct ChatPaneSizes
{
    int roomsHeight = 1;
    int messagesHeight = 1;
    int messagesWrapWidth = 1;
};

// What every key does in chat, without a terminal: the screen turns terminal
// events into key names ("j", "C-d", "Enter", "Esc") and draws this state.
// Navigation keys go through ChatKeyParser, the compose box is a vim::Editor
// and ':' opens a one-line command line, as in DiffView.
class ChatInput
{
public:
    explicit ChatInput(ChatSession& sessionToUse);

    void handleKey(const std::string& key);

    // Called before each frame is drawn: rewraps the messages when the width
    // changed and scrolls both panes to keep their cursors on screen.
    void layOutPanes(const ChatPaneSizes& sizes);

    // After a room's file changed: new messages show, and the cursor follows
    // them if it was on the newest one.
    void reloadRoom(const std::string& name);

    bool hasQuit() const;
    ChatPane getFocusedPane() const;
    bool isComposing() const;
    bool isShowingHelp() const;
    bool isCommandLineOpen() const;
    bool hasUnsentDraft() const;
    const LineEdit& getCommandLine() const;
    vim::Editor& getEditor();
    const ChatMessagePane& getMessagePane() const;
    int getRoomsTopRow() const;
    const ChatStatus& getStatus() const;
    std::string getPendingKeys() const;
    const ChatSession& getSession() const;
    int getSentCount() const;

private:
    void handleNavigationKey(const std::string& key);
    void runCommand(const ChatCommand& command);
    std::optional<int> findMovementDistance(const ChatCommand& command) const;
    void runPaneCommand(ChatAction action);
    void runOverlayCommand(ChatAction action);
    void toggleFocusedPane();
    void openRoomOrComposeMessage();
    void moveInFocusedPane(int delta);
    int getFocusedPaneHalfHeight() const;
    void selectRoomByOffset(int offset);
    void showActiveRoomFromNewestMessage();
    std::vector<ChatMessageRow> buildActiveRoomRows() const;

    void openComposeBox();
    void openComposeBoxInInsertMode();
    void showComposeBox();
    bool isLeavingComposeBox(const std::string& key) const;
    void handleComposeKey(const std::string& key);
    void applyEditorOutcome(vim::Outcome outcome);
    // False when the message could not be written; the box stays open.
    bool sendComposedMessage();
    void discardComposedMessage();
    void closeComposeBox();

    void handleCommandLineKey(const std::string& key);
    void runCommandLine(const std::string& line);

    void say(std::string text, bool isError);
    void quit(std::string_view via);

    ChatSession& session;
    ChatKeyParser keys;
    vim::Editor editor;
    LineEdit commandLine;
    ChatMessagePane messagePane;
    ChatPane focus = ChatPane::rooms;
    ChatStatus status;
    bool isComposeBoxOpen = false;
    bool isHelpOpen = false;
    bool isCommandLineShown = false;
    bool isQuitRequested = false;
    int roomsTopRow = 0;
    ChatPaneSizes paneSizes;
    int sentCount = 0;
};
} // namespace iac
