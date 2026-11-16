#include <MessageAddressing.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto agentName = "eacp-rebase-1";
} // namespace

auto tExactMention =
    test("MessageAddressing/an exact mention addresses the agent") = []
{
    check(isMessageAddressedTo("@eacp-rebase-1 please rebase", agentName));
    check(isMessageAddressedTo("done, @eacp-rebase-1", agentName));
    check(isMessageAddressedTo("@eacp-rebase-1", agentName));
};

auto tLongerNameMention = test(
    "MessageAddressing/a mention of a longer name does not address the agent") = []
{
    check(!isMessageAddressedTo("@eacp-rebase-109 please rebase", agentName));
    check(!isMessageAddressedTo("@eacp-rebase-1_b look", agentName));
    check(!isMessageAddressedTo("@eacp-rebase-1x look", agentName));
    check(!isMessageAddressedTo("@xeacp-rebase-1 look", agentName));
    check(!isMessageAddressedTo("mail me@eacp-rebase-1 later", agentName));
};

auto tPunctuationAfterMention = test(
    "MessageAddressing/punctuation after a mention still addresses the agent") = []
{
    check(isMessageAddressedTo("@eacp-rebase-1: rebase", agentName));
    check(isMessageAddressedTo("thanks @eacp-rebase-1.", agentName));
    check(isMessageAddressedTo("@eacp-rebase-1, @other: both of you", agentName));
    check(isMessageAddressedTo("(@eacp-rebase-1) see above", agentName));
};

auto tMentionCase = test("MessageAddressing/mentions ignore case") = []
{
    check(isMessageAddressedTo("@EACP-Rebase-1 hi", agentName));
    check(isMessageAddressedTo("@eacp-rebase-1 hi", "EACP-REBASE-1"));
};

auto tLaterMentionMatches =
    test("MessageAddressing/a later exact mention matches after a longer one") = []
{ check(isMessageAddressedTo("@eacp-rebase-109 and @eacp-rebase-1", agentName)); };

auto tMentionAll = test("MessageAddressing/@all addresses every agent") = []
{
    check(isMessageAddressedTo("@all build is red", agentName));
    check(isMessageAddressedTo("heads up @ALL.", agentName));
    check(!isMessageAddressedTo("@allison look", agentName));
};

auto tNoMention =
    test("MessageAddressing/a message with no mention addresses no agent") = []
{
    check(!isMessageAddressedTo("eacp-rebase-1 is done", agentName));
    check(!isMessageAddressedTo("build is green", agentName));
    check(!isMessageAddressedTo("", agentName));
};

auto tSkippedLine = test(
    "MessageAddressing/the skipped line counts the messages and names the read that shows them") =
    []
{
    constexpr auto skippedCount = std::size_t {14};
    constexpr auto readCount = std::size_t {15};
    check(describeSkippedMessages({skippedCount, "03:12:05", readCount})
          == "iac: skipped 14 messages not addressed to you, the first at "
             "03:12:05; iac read -n 15 shows them");
    check(describeSkippedMessages({1, "03:12:05", 2})
          == "iac: skipped 1 message not addressed to you, the first at "
             "03:12:05; iac read -n 2 shows them");
};
