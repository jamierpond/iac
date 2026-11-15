#include "Commands.h"
#include "RemoteRoom.h"
#include "StoreRooms.h"

#include <cstdio>

namespace iac
{
namespace
{
int printRoomNames(const Room& room)
{
    auto error = std::error_code {};
    const auto names = findRoomNamesInStore(room, error);
    if (error)
    {
        std::fprintf(
            stderr, "iac: could not list %s\n", getRoomDirectory(room).c_str());
        return 1;
    }

    for (const auto& name: names)
    {
        if (name.empty())
            std::printf("(default)\n");
        else
            std::printf("#%s\n", name.c_str());
    }
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
