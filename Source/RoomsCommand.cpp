#include "Commands.h"
#include "RemoteRoom.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace iac
{
namespace
{
struct StoreRooms
{
    bool hasDefaultRoom = false;
    std::vector<std::string> names;
};

// A room is one document file in the store, so listing rooms is listing
// files: "messages.json" is the default room, "room-<name>.json" the rest.
StoreRooms findRoomsInStore(const std::filesystem::path& root,
                            std::error_code& error)
{
    auto rooms = StoreRooms {};
    for (const auto& entry: std::filesystem::directory_iterator {root, error})
    {
        const auto file = entry.path().filename().string();
        if (!file.ends_with(documentFileExtension))
            continue;
        const auto stem = file.substr(0, file.size() - documentFileExtension.size());
        if (stem == defaultRoomDocumentName)
            rooms.hasDefaultRoom = true;
        else if (stem.starts_with(namedRoomDocumentPrefix))
            rooms.names.push_back(stem.substr(namedRoomDocumentPrefix.size()));
    }
    return rooms;
}

int printRoomNames(const Room& room)
{
    const auto root = std::filesystem::path {getRoomDirectory(room).str()};
    auto error = std::error_code {};
    auto rooms = findRoomsInStore(root, error);
    if (error)
    {
        std::fprintf(
            stderr, "iac: could not list %s\n", getRoomDirectory(room).c_str());
        return 1;
    }

    std::sort(rooms.names.begin(), rooms.names.end());
    if (rooms.hasDefaultRoom)
        std::printf("(default)\n");
    for (const auto& name: rooms.names)
        std::printf("#%s\n", name.c_str());
    return 0;
}
} // namespace

int runRoomsCommand(const Room& room)
{
    if (room.isRemote())
        return runOverSsh(room, {"rooms"});
    return printRoomNames(room);
}
} // namespace iac
