#include "Commands.h"
#include "Message.h"
#include "RemoteRoom.h"

#include <cstdlib>

namespace iac
{
namespace
{
constexpr auto defaultReadCount = std::size_t {20};
constexpr auto decimalBase = 10;

std::size_t parseReadCount(const std::vector<std::string>& args)
{
    auto count = defaultReadCount;
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "-n" && i + 1 < args.size())
            count = std::strtoul(args[++i].c_str(), nullptr, decimalBase);
    }
    return count;
}

int printLastMessages(const Room& room, std::size_t count)
{
    auto document =
        openRoomDatabase(room).document<Messages>(getRoomDocumentName(room));
    for (const auto& message: getLastMessages(document.peek(), count))
        printMessage(message);
    return 0;
}
} // namespace

int runReadCommand(const Room& room, const std::vector<std::string>& arguments)
{
    const auto count = parseReadCount(arguments);

    if (room.isRemote())
        return runOverSsh(room, {"read", "-n", std::to_string(count)});
    return printLastMessages(room, count);
}
} // namespace iac
