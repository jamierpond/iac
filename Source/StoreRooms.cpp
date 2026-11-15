#include "StoreRooms.h"

#include <algorithm>
#include <filesystem>
#include <optional>

namespace iac
{
namespace
{
// "messages.json" is the default room, named "", and "room-<name>.json" the rest.
std::optional<std::string> findRoomNameOfDocumentFile(const std::filesystem::path& file)
{
    if (file.extension().string() != documentFileExtension)
        return std::nullopt;
    const auto stem = file.stem().string();
    if (stem == defaultRoomDocumentName)
        return std::string {};
    if (stem.starts_with(namedRoomDocumentPrefix))
        return stem.substr(namedRoomDocumentPrefix.size());
    return std::nullopt;
}
} // namespace

// A room is one document file in the store, so listing rooms is listing files.
std::vector<std::string> findRoomNamesInStore(const Room& store, std::error_code& error)
{
    const auto root = std::filesystem::path {getRoomDirectory(store).str()};
    auto names = std::vector<std::string> {};
    for (const auto& entry: std::filesystem::directory_iterator {root, error})
    {
        if (auto name = findRoomNameOfDocumentFile(entry.path().filename()))
            names.push_back(std::move(*name));
    }
    std::sort(names.begin(), names.end());
    return names;
}
} // namespace iac
