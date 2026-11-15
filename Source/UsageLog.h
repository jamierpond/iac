#pragma once

#include <SessionLog/EventLine.h>

#include <initializer_list>
#include <string_view>

namespace iac
{
// Starts this launch's log at pond/iac/logs/<startEpochMs>.log, keeps the
// newest 50, and logs app.launch with `mode` (the command, or chat).
void startUsageLog(std::string_view mode);

// Appends `HH:MM:SS.mmm | <event> key=value ...` to the session log. Only the
// file: stdout belongs to the chat screen and to agents reading iac's output.
void logEvent(std::string_view event,
              std::initializer_list<sessionlog::Field> fields = {});
} // namespace iac
