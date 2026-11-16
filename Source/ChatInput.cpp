#include "ChatInput.h"

#include "ComposeLine.h"
#include "Publish.h"
#include "UsageLog.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace iac
{
namespace
{
// gg and G move this far, which the panes clamp to their first and last rows.
constexpr auto distanceBeyondEitherEnd = 1000000;

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

std::string_view getPaneName(ChatPane pane)
{
    if (pane == ChatPane::rooms)
        return "rooms";
    return "messages";
}

bool isCommandLineCancelKey(const std::string& key, const LineEdit& line)
{
    return key == "Esc" || key == "C-c"
           || (key == "Backspace" && line.text().empty());
}
} // namespace

ChatInput::ChatInput(ChatSession& sessionToUse)
    : session(sessionToUse)
{
    showActiveRoomFromNewestMessage();
}

void ChatInput::handleKey(const std::string& key)
{
    if (key.empty())
        return;
    if (isComposeBoxOpen)
    {
        handleComposeKey(key);
        return;
    }
    if (isHelpOpen)
    {
        isHelpOpen = false;
        return;
    }
    if (isCommandLineShown)
    {
        handleCommandLineKey(key);
        return;
    }
    handleNavigationKey(key);
}

void ChatInput::handleNavigationKey(const std::string& key)
{
    const auto keysBefore = keys.getPendingKeys();
    const auto command = keys.feed(key);
    if (!command)
        return;
    logEvent("chat.key",
             {{"keys", keysBefore + key},
              {"action", getChatActionName(command->action)},
              {"count", command->count},
              {"pane", getPaneName(focus)}});
    runCommand(*command);
}

void ChatInput::runCommand(const ChatCommand& command)
{
    if (const auto distance = findMovementDistance(command))
    {
        moveInFocusedPane(*distance);
        return;
    }
    runPaneCommand(command.action);
}

std::optional<int> ChatInput::findMovementDistance(const ChatCommand& command) const
{
    switch (command.action)
    {
        case ChatAction::down:
            return command.count;
        case ChatAction::up:
            return -command.count;
        case ChatAction::top:
            return -distanceBeyondEitherEnd;
        case ChatAction::bottom:
            return distanceBeyondEitherEnd;
        case ChatAction::halfPageDown:
            return command.count * getFocusedPaneHalfHeight();
        case ChatAction::halfPageUp:
            return -command.count * getFocusedPaneHalfHeight();
        default:
            return std::nullopt;
    }
}

void ChatInput::runPaneCommand(ChatAction action)
{
    switch (action)
    {
        case ChatAction::focusRooms:
            focus = ChatPane::rooms;
            break;
        case ChatAction::focusMessages:
            focus = ChatPane::messages;
            break;
        case ChatAction::togglePane:
            toggleFocusedPane();
            break;
        case ChatAction::open:
            openRoomOrComposeMessage();
            break;
        default:
            runOverlayCommand(action);
            break;
    }
}

void ChatInput::runOverlayCommand(ChatAction action)
{
    switch (action)
    {
        case ChatAction::compose:
            openComposeBox();
            break;
        case ChatAction::composeInInsertMode:
            openComposeBoxInInsertMode();
            break;
        case ChatAction::commandLine:
            commandLine.set({});
            isCommandLineShown = true;
            break;
        case ChatAction::help:
            isHelpOpen = true;
            break;
        case ChatAction::quit:
            quit("q");
            break;
        default:
            break;
    }
}

void ChatInput::toggleFocusedPane()
{
    if (focus == ChatPane::rooms)
        focus = ChatPane::messages;
    else
        focus = ChatPane::rooms;
}

// Enter opens the room in the list and, in the messages, writes one, as dv's
// Enter opens a file in the tree and comments on a line in the diff.
void ChatInput::openRoomOrComposeMessage()
{
    if (focus == ChatPane::rooms)
        focus = ChatPane::messages;
    else
        openComposeBox();
}

int ChatInput::getFocusedPaneHalfHeight() const
{
    const auto height =
        focus == ChatPane::rooms ? paneSizes.roomsHeight : paneSizes.messagesHeight;
    return std::max(1, height / 2);
}

void ChatInput::moveInFocusedPane(int delta)
{
    if (focus == ChatPane::rooms)
    {
        selectRoomByOffset(delta);
        return;
    }
    messagePane.moveCursor(delta);
}

void ChatInput::selectRoomByOffset(int offset)
{
    auto& rooms = session.getRooms();
    const auto before = rooms.getActiveRoomName();
    rooms.selectRoomByOffset(offset);
    if (rooms.getActiveRoomName() == before)
        return;
    showActiveRoomFromNewestMessage();
    logEvent("chat.room",
             {{"room", getRoomLabel(rooms.getActiveRoomName())}, {"via", "list"}});
}

std::vector<ChatMessageRow> ChatInput::buildActiveRoomRows() const
{
    return buildChatMessageRows(session.getRooms().getActiveRoomMessages(),
                                paneSizes.messagesWrapWidth);
}

void ChatInput::showActiveRoomFromNewestMessage()
{
    messagePane.showRowsFromNewest(buildActiveRoomRows());
}

void ChatInput::layOutPanes(const ChatPaneSizes& sizes)
{
    const auto isRewrapNeeded =
        sizes.messagesWrapWidth != paneSizes.messagesWrapWidth;
    paneSizes = sizes;
    if (isRewrapNeeded)
        messagePane.replaceRows(buildActiveRoomRows());

    messagePane.scrollToShowCursor(paneSizes.messagesHeight);
    const auto& rooms = session.getRooms();
    roomsTopRow =
        calculateTopRowShowingCursor(rooms.getActiveRoomIndex(),
                                     roomsTopRow,
                                     paneSizes.roomsHeight,
                                     static_cast<int>(rooms.getRoomNames().size()));
}

void ChatInput::reloadRoom(const std::string& name)
{
    session.reloadRoom(name);
    if (name == session.getRooms().getActiveRoomName())
        messagePane.replaceRows(buildActiveRoomRows());
}

// As dv opens its comment box: a new message in insert mode, a draft in
// normal mode at its end.
void ChatInput::openComposeBox()
{
    if (editor.text().empty())
        editor.feed("i");
    else
    {
        editor.feed("G");
        editor.feed("$");
    }
    showComposeBox();
}

void ChatInput::openComposeBoxInInsertMode()
{
    editor.feed("G");
    editor.feed("A");
    showComposeBox();
}

void ChatInput::showComposeBox()
{
    isComposeBoxOpen = true;
    focus = ChatPane::messages;
    logEvent("chat.compose", {{"draft", hasUnsentDraft()}});
}

// Esc in normal mode, which vim ignores, closes the box and keeps the draft.
bool ChatInput::isLeavingComposeBox(const std::string& key) const
{
    if (key != "Esc")
        return false;
    return editor.mode() == vim::Mode::normal && !editor.hasPendingKeys();
}

void ChatInput::handleComposeKey(const std::string& key)
{
    if (isLeavingComposeBox(key))
    {
        closeComposeBox();
        return;
    }

    const auto modeBefore = editor.mode();
    const auto outcome = editor.feed(key);
    if (editor.mode() != modeBefore)
    {
        logEvent("chat.editor_mode",
                 {{"from", getEditorModeName(modeBefore)},
                  {"to", getEditorModeName(editor.mode())}});
    }
    applyEditorOutcome(outcome);
}

// dv's comment box keys: :w sends, :wq, :x and C-s send and close, :q closes,
// :q! drops the text.
void ChatInput::applyEditorOutcome(vim::Outcome outcome)
{
    switch (outcome)
    {
        case vim::Outcome::save:
            sendComposedMessage();
            break;
        case vim::Outcome::saveAndClose:
            if (sendComposedMessage())
                closeComposeBox();
            break;
        case vim::Outcome::close:
            closeComposeBox();
            break;
        case vim::Outcome::discard:
            discardComposedMessage();
            break;
        case vim::Outcome::none:
        case vim::Outcome::paste:
            break;
    }
}

bool ChatInput::sendComposedMessage()
{
    const auto result = session.sendComposedMessage(editor.text());
    if (result.isError)
    {
        logEvent("chat.error", {{"reason", result.status}});
        say(result.status, true);
        return false;
    }
    if (!result.wasSent)
        return true;

    ++sentCount;
    editor.setText({});
    showActiveRoomFromNewestMessage();
    logEvent("chat.submit",
             {{"kind", "message"},
              {"room", getRoomLabel(session.getRooms().getActiveRoomName())}});
    return true;
}

void ChatInput::discardComposedMessage()
{
    editor.setText({});
    closeComposeBox();
    logEvent("chat.discard");
    say("Discarded", false);
}

void ChatInput::closeComposeBox()
{
    isComposeBoxOpen = false;
    if (editor.mode() == vim::Mode::insert)
        editor.feed("Esc");
}

void ChatInput::handleCommandLineKey(const std::string& key)
{
    if (isCommandLineCancelKey(key, commandLine))
    {
        isCommandLineShown = false;
        return;
    }
    if (key == "Enter")
    {
        isCommandLineShown = false;
        runCommandLine(commandLine.text());
        return;
    }
    commandLine.feed(key);
}

void ChatInput::runCommandLine(const std::string& line)
{
    const auto action = parseChatCommandLine(line);
    switch (action.kind)
    {
        case ChatCommandKind::joinRoom:
            session.getRooms().selectRoom(action.argument);
            session.reloadRoom(action.argument);
            showActiveRoomFromNewestMessage();
            say("Joined " + getRoomLabel(action.argument), false);
            logEvent("chat.room",
                     {{"room", getRoomLabel(action.argument)}, {"via", "join"}});
            break;
        case ChatCommandKind::showHelp:
            isHelpOpen = true;
            break;
        case ChatCommandKind::quit:
            quit(":q");
            break;
        case ChatCommandKind::showError:
            say(action.argument, true);
            break;
        case ChatCommandKind::nothing:
            break;
    }
}

void ChatInput::say(std::string text, bool isError)
{
    status = {std::move(text), isError, getMillisecondsSinceEpoch()};
}

void ChatInput::quit(std::string_view via)
{
    logEvent("chat.quit", {{"via", via}});
    isQuitRequested = true;
}

bool ChatInput::hasQuit() const
{
    return isQuitRequested;
}

ChatPane ChatInput::getFocusedPane() const
{
    return focus;
}

bool ChatInput::isComposing() const
{
    return isComposeBoxOpen;
}

bool ChatInput::isShowingHelp() const
{
    return isHelpOpen;
}

bool ChatInput::isCommandLineOpen() const
{
    return isCommandLineShown;
}

bool ChatInput::hasUnsentDraft() const
{
    return !trimComposedMessage(editor.text()).empty();
}

const LineEdit& ChatInput::getCommandLine() const
{
    return commandLine;
}

vim::Editor& ChatInput::getEditor()
{
    return editor;
}

const ChatMessagePane& ChatInput::getMessagePane() const
{
    return messagePane;
}

int ChatInput::getRoomsTopRow() const
{
    return roomsTopRow;
}

const ChatStatus& ChatInput::getStatus() const
{
    return status;
}

std::string ChatInput::getPendingKeys() const
{
    return keys.getPendingKeys();
}

const ChatSession& ChatInput::getSession() const
{
    return session;
}

int ChatInput::getSentCount() const
{
    return sentCount;
}
} // namespace iac
