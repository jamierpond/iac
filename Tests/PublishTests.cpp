#include "TemporaryStore.h"

#include <Publish.h>
#include <StoreRooms.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

auto tKeysSortByTime = test("Publish/message keys sort in time order") = []
{
    constexpr auto earlier = std::int64_t {999};
    constexpr auto later = std::int64_t {1000};
    check(makeMessageKey(earlier) < makeMessageKey(later));
};

auto tPublishThenRead = test("Publish/messages read back in the order they were published") = []
{
    const auto store = testing::TemporaryStore {};
    const auto room = store.getRoom();
    auto timestamp = std::int64_t {1000};
    for (const auto* text: {"one", "two", "three"})
        check(publishToLocalRoom(room, Message {"jamie", text, "/here", ++timestamp}));

    auto document = openRoomDatabase(room).document<Messages>(getRoomDocumentName(room));
    const auto last = getLastMessages(document.peek(), 10);
    check(last.size() == 3);
    check(last[0].text == "one");
    check(last[2].text == "three");
    check(last[0].sender == "jamie");
};

auto tPublishNamedRoom = test("Publish/a named room is its own document, listed by name") = []
{
    const auto store = testing::TemporaryStore {};
    constexpr auto timestamp = std::int64_t {1000};
    check(publishToLocalRoom(store.getRoom(), Message {"a", "hi", {}, timestamp}));
    check(publishToLocalRoom(store.getRoom("design"), Message {"b", "ho", {}, timestamp}));

    auto error = std::error_code {};
    const auto names = findRoomNamesInStore(store.getRoom(), error);
    check(!error);
    check(names == std::vector<std::string> {"", "design"});
};

auto tWindowsDriveIsLocal = test("Room/a Windows drive path is a local store, not a host") = []
{
    check(!parseRoomSpecification("C:\\Users\\me\\iac").isRemote());
    check(!parseRoomSpecification("C:/Users/me/iac#design").isRemote());
    check(parseRoomSpecification("c:").isRemote() == false);
    check(parseRoomSpecification("box:dir").isRemote());
    check(parseRoomSpecification("me@box:").sshTarget == "me@box");
};
