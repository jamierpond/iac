#include "Store.h"
#include "Platform.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <random>

namespace iac
{
// scp's rule on the store part: a colon before the first slash splits host
// from path. An empty path ("host:") means the remote default store. The
// first '#' starts the room name — store directories containing '#' need
// IAC_DIR instead.
Room parseRoomSpec(const std::string& spec)
{
    auto store = spec;
    auto name = std::string {};
    if (const auto hash = spec.find('#'); hash != std::string::npos)
    {
        store = spec.substr(0, hash);
        name = spec.substr(hash + 1);
    }
    const auto colon = store.find(':');
    if (colon != std::string::npos && colon > 0 && store.find('/') > colon)
        return {store.substr(0, colon), store.substr(colon + 1), name};
    return {{}, store, name};
}

// Room names appear in file names and forwarded shell commands, so keep them
// to characters safe in both.
bool validRoomName(const std::string& name)
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

Room activeRoom;

// Expanding ~ here, rather than in a shell, lets a forwarded room spec carry
// a home-relative store as a plain argument.
eacp::FilePath roomDirectory()
{
    const auto& directory = activeRoom.directory;
    if (directory.empty())
        return eacp::FilePath::homeDirectory() / ".iac";
    if (directory == "~")
        return eacp::FilePath::homeDirectory();
    if (directory.starts_with("~/"))
        return eacp::FilePath::homeDirectory() / directory.substr(2);
    return directory;
}

emberstore::Database openRoom() { return emberstore::Database {roomDirectory()}; }

// The default room keeps the historical "messages" document, so existing
// stores need no migration; named rooms get their own file alongside it.
std::string roomDocumentName()
{
    if (activeRoom.name.empty())
        return "messages";
    return "room-" + activeRoom.name;
}

std::string defaultSender()
{
    for (const auto* var: {"IAC_NAME", "USER", "USERNAME"})
        if (const auto* value = std::getenv(var))
            return value;
    return "anon";
}

std::int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

// Zero-padded epoch millis so the collection's key order is chronological; a
// random suffix keeps two agents publishing in the same millisecond apart.
std::string makeKey(std::int64_t timestamp)
{
    const auto suffix = std::random_device {}() & 0xffffu;
    char key[32];
    std::snprintf(
        key, sizeof(key), "%015lld-%04x", static_cast<long long>(timestamp), suffix);
    return key;
}

std::string currentDirectory()
{
    auto error = std::error_code {};
    const auto path = std::filesystem::current_path(error);
    if (error)
        return {};
    return eacp::FilePath {path}.str();
}

// "~/projects/iac" reads better than the poster's full home prefix — but only
// abbreviate when the home matches ours, so paths from another user's room
// stay intact.
std::string abbreviateHome(const std::string& path)
{
    const auto& home = eacp::FilePath::homeDirectory().str();
    if (!home.empty() && path.starts_with(home))
        return "~" + path.substr(home.size());
    return path;
}

std::string formatTime(std::int64_t timestamp)
{
    const auto local = localTime(static_cast<std::time_t>(timestamp / 1000));
    char text[16];
    std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return text;
}

int publish(const std::string& text,
            const std::string& sender,
            const std::string& cwd)
{
    auto messages = openRoom().collection<Message>(roomDocumentName());
    const auto message = Message {sender, text, cwd, nowMs()};

    if (!messages.doc(makeKey(message.timestamp)).set(message))
    {
        std::fprintf(
            stderr, "iac: could not write to %s\n", roomDirectory().c_str());
        return 1;
    }
    return 0;
}

// A room is one document file in the store, so listing rooms is listing
// files: "messages.json" is the default room, "room-<name>.json" the rest.
std::optional<RoomList> findRooms()
{
    const auto root = std::filesystem::path {roomDirectory().str()};
    auto error = std::error_code {};
    auto names = std::vector<std::string> {};
    auto haveDefault = false;
    for (const auto& entry: std::filesystem::directory_iterator {root, error})
    {
        const auto file = entry.path().filename().string();
        if (!file.ends_with(".json"))
            continue;
        const auto stem = file.substr(0, file.size() - 5);
        if (stem == "messages")
            haveDefault = true;
        else if (stem.starts_with("room-"))
            names.push_back(stem.substr(5));
    }
    if (error)
        return std::nullopt;

    std::sort(names.begin(), names.end());
    return RoomList {haveDefault, names};
}
} // namespace iac
