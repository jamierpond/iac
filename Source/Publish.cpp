#include "Publish.h"

#include <chrono>
#include <cstdio>
#include <random>

namespace iac
{
namespace
{
constexpr auto messageKeySuffixMask = 0xffffu;
constexpr auto messageKeyCapacity = 32;
} // namespace

std::int64_t getMillisecondsSinceEpoch()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

std::string makeMessageKey(std::int64_t timestamp)
{
    const auto suffix = std::random_device {}() & messageKeySuffixMask;
    const auto timestampForFormat = static_cast<long long>(timestamp);
    char key[messageKeyCapacity];
    std::snprintf(key, sizeof(key), "%015lld-%04x", timestampForFormat, suffix);
    return key;
}

bool publishToLocalRoom(const Room& room, const Message& message)
{
    auto messages =
        openRoomDatabase(room).collection<Message>(getRoomDocumentName(room));
    return messages.doc(makeMessageKey(message.timestamp)).set(message);
}
} // namespace iac
