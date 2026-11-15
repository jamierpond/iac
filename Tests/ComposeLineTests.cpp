#include <ComposeLine.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

auto tPlainMessage = test("ComposeLine/plain text is a message, trimmed") = []
{
    const auto action = parseComposedText("  hello there \n");
    check(action.kind == ComposeActionKind::sendMessage);
    check(action.argument == "hello there");
};

auto tBlank = test("ComposeLine/blank text does nothing") = []
{
    check(parseComposedText(" \n ").kind == ComposeActionKind::nothing);
};

auto tJoin = test("ComposeLine/join takes #name or name, and rejects bad names") = []
{
    check(parseComposedText("/join #db-design").argument == "db-design");
    check(parseComposedText("/join ops").kind == ComposeActionKind::joinRoom);
    check(parseComposedText("/join #Bad Name").kind == ComposeActionKind::showError);
    check(parseComposedText("/join").kind == ComposeActionKind::showError);
};

auto tCommands = test("ComposeLine/rooms, quit, unknown commands and // escapes") = []
{
    check(parseComposedText("/rooms").kind == ComposeActionKind::listRooms);
    check(parseComposedText("/quit").kind == ComposeActionKind::quit);
    check(parseComposedText("/joinx").kind == ComposeActionKind::showError);
    const auto escaped = parseComposedText("//etc is a path");
    check(escaped.kind == ComposeActionKind::sendMessage);
    check(escaped.argument == "/etc is a path");
};

auto tMentions = test("ComposeLine/mentions are @role, not emails, without trailing dots") = []
{
    const auto text = std::string {"@iac-chat see a@b.com and @dv.review."};
    const auto mentions = findMentions(text);
    check(mentions.size() == 2);
    check(text.substr(mentions[0].start, mentions[0].length) == "@iac-chat");
    check(text.substr(mentions[1].start, mentions[1].length) == "@dv.review");
    check(findMentions("lone @ sign").empty());
};
