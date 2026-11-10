#include "Room.h"

#include <algorithm>
#include <cctype>

namespace iac
{
namespace
{
constexpr auto homeRelativePrefix = std::string_view {"~/"};

bool doesColonSeparateHost(const std::string& store, std::size_t colon)
{
    return colon != std::string::npos && colon > 0 && store.find('/') > colon;
}
} // namespace

// scp's rule on the store part: a colon before the first slash splits host
// from path. An empty path ("host:") means the remote default store. The
// first '#' starts the room name — store directories containing '#' need
// IAC_DIR instead.
Room parseRoomSpecification(const std::string& specification)
{
    auto store = specification;
    auto name = std::string {};
    if (const auto hash = specification.find('#'); hash != std::string::npos)
    {
        store = specification.substr(0, hash);
        name = specification.substr(hash + 1);
    }
    const auto colon = store.find(':');
    if (doesColonSeparateHost(store, colon))
        return {store.substr(0, colon), store.substr(colon + 1), name};
    return {{}, store, name};
}

// Room names appear in file names and forwarded shell commands, so keep them
// to characters safe in both.
bool isValidRoomName(const std::string& name)
{
    return !name.empty()
           && std::all_of(name.begin(),
                          name.end(),
                          [](unsigned char c)
                          {
                              return std::islower(c) != 0 || std::isdigit(c) != 0
                                     || c == '-' || c == '_';
                          });
}

// Expanding ~ here, rather than in a shell, lets a forwarded room spec carry
// a home-relative store as a plain argument.
eacp::FilePath getRoomDirectory(const Room& room)
{
    const auto& directory = room.directory;
    if (directory.empty())
        return eacp::FilePath::homeDirectory() / ".iac";
    if (directory == "~")
        return eacp::FilePath::homeDirectory();
    if (directory.starts_with(homeRelativePrefix))
    {
        const auto pathInsideHome = directory.substr(homeRelativePrefix.size());
        return eacp::FilePath::homeDirectory() / pathInsideHome;
    }
    return directory;
}

emberstore::Database openRoomDatabase(const Room& room)
{
    return emberstore::Database {getRoomDirectory(room)};
}

// The default room keeps the historical "messages" document, so existing
// stores need no migration; named rooms get their own file alongside it.
std::string getRoomDocumentName(const Room& room)
{
    if (room.name.empty())
        return std::string {defaultRoomDocumentName};
    return std::string {namedRoomDocumentPrefix} + room.name;
}
} // namespace iac
