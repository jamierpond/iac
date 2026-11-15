#include <Message.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto firstTimestamp = std::int64_t {1000};

Messages makeMessages(std::initializer_list<std::pair<std::string, std::string>> keysAndTexts)
{
    auto messages = Messages {};
    for (const auto& [key, text]: keysAndTexts)
        messages[key] = Message {"sender", text, {}, firstTimestamp};
    return messages;
}
} // namespace

auto tLastMessagesOldestFirst = test("Message/getLastMessages keeps the newest, oldest first") = []
{
    const auto messages = makeMessages({{"003", "c"}, {"001", "a"}, {"002", "b"}});
    constexpr auto lastTwo = std::size_t {2};
    const auto last = getLastMessages(messages, lastTwo);
    check(last.size() == 2);
    check(last[0].text == "b");
    check(last[1].text == "c");
};

auto tLastMessagesMoreThanThere = test("Message/getLastMessages with a count past the end returns all") = []
{
    constexpr auto manyMore = std::size_t {50};
    check(getLastMessages(makeMessages({{"001", "a"}}), manyMore).size() == 1);
};

auto tFormatLine = test("Message/formatMessageLine names the sender and directory") = []
{
    const auto message = Message {"planner", "deploy is green", "/srv/app", firstTimestamp};
    const auto line = formatMessageLine(message);
    check(line.starts_with("["));
    check(line.ends_with("] planner (/srv/app): deploy is green"));
};

auto tCursorSkipsExisting = test("Message/MessageCursor yields only messages after it was made") = []
{
    auto messages = makeMessages({{"001", "old"}});
    auto cursor = MessageCursor {messages};
    check(cursor.takeNewMessages(messages).empty());

    messages["002"] = Message {"a", "new one", {}, firstTimestamp};
    messages["003"] = Message {"b", "new two", {}, firstTimestamp};
    check(cursor.countNewMessages(messages) == 2);

    const auto fresh = cursor.takeNewMessages(messages);
    check(fresh.size() == 2);
    check(fresh[0].text == "new one");
    check(fresh[1].text == "new two");
    check(cursor.countNewMessages(messages) == 0);
};
