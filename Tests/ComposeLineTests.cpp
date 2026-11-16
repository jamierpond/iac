#include <ComposeLine.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

auto tMessageTrimmed =
    test("ComposeLine/a composed message is sent trimmed; blank is nothing") = []
{
    check(trimComposedMessage("  hello there \n") == "hello there");
    check(trimComposedMessage("/etc is a path") == "/etc is a path");
    check(trimComposedMessage(" \n ").empty());
};

auto tJoin =
    test("ComposeLine/:join takes #name or name, and rejects bad names") = []
{
    check(parseChatCommandLine("join #db-design").argument == "db-design");
    check(parseChatCommandLine("join ops").kind == ChatCommandKind::joinRoom);
    check(parseChatCommandLine("join #Bad Name").kind == ChatCommandKind::showError);
    check(parseChatCommandLine("join").kind == ChatCommandKind::showError);
};

auto tCommands =
    test("ComposeLine/:q, :quit, :help, unknown and blank command lines") = []
{
    check(parseChatCommandLine("q").kind == ChatCommandKind::quit);
    check(parseChatCommandLine("quit!").kind == ChatCommandKind::quit);
    check(parseChatCommandLine("help").kind == ChatCommandKind::showHelp);
    check(parseChatCommandLine("joinx").kind == ChatCommandKind::showError);
    check(parseChatCommandLine("  ").kind == ChatCommandKind::nothing);
};

auto tMentions =
    test("ComposeLine/mentions are @role, not emails, without trailing dots") = []
{
    const auto text = std::string {"@iac-chat see a@b.com and @dv.review."};
    const auto mentions = findMentions(text);
    check(mentions.size() == 2);
    check(text.substr(mentions[0].start, mentions[0].length) == "@iac-chat");
    check(text.substr(mentions[1].start, mentions[1].length) == "@dv.review");
    check(findMentions("lone @ sign").empty());
};
