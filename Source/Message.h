#pragma once

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace iac
{
struct Message
{
    std::string sender;
    std::string text;
    std::string cwd;
    std::int64_t timestamp = 0;

    MIRO_REFLECT(sender, text, cwd, timestamp)
};

using Messages = std::map<std::string, Message>;

std::string abbreviateHome(const std::string& path);

// When the message was sent, as local "HH:MM:SS".
std::string formatTimeOfDay(const Message& message);

// "[14:32:07] planner (~/projects/api): deploy is green", no newline.
std::string formatMessageLine(const Message& message);

void printMessage(const Message& message);

std::vector<Message> getLastMessages(const Messages& messages, std::size_t count);

// Remembers the newest message key seen, so a room re-read after a change
// yields only what was published since: how monitor and chat follow a room.
class MessageCursor
{
public:
    // Everything already in `existing` counts as seen.
    explicit MessageCursor(const Messages& existing);

    std::vector<Message> takeNewMessages(const Messages& messages);

    std::size_t countNewMessages(const Messages& messages) const;

    void markAllSeen(const Messages& messages);

private:
    std::string lastSeenKey;
};
} // namespace iac
