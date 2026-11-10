#pragma once

#include <eacp/Core/Core.h>
#include <emberstore/Emberstore.h>

#include <cstdint>
#include <map>
#include <string>

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

void printMessage(const Message& message);
} // namespace iac
