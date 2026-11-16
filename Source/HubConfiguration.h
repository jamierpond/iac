#pragma once

#include "Room.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace iac
{
class ConfigurationFileError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// ~/.config/iac.json, the same path on every OS.
eacp::FilePath getConfigurationFilePath();

// The room named by the file's "hub" key, or nullopt when the file or the key
// is missing. Throws ConfigurationFileError when the file is not a JSON object
// or "hub" is not a non-empty string.
std::optional<Room> readHubRoomFromConfigurationFile(const eacp::FilePath& path);

bool isHubThisMachine(const Room& hub, const std::string& shortHostname);
} // namespace iac
