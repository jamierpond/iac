#include <ChatKeymap.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

auto tSingle = test("ChatKeymap/single keys map straight to commands") = []
{
    auto keys = ChatKeyParser {};
    auto command = keys.feed("j");

    check(command && command->action == ChatAction::down && command->count == 1);
};

auto tCount = test("ChatKeymap/a count prefix repeats") = []
{
    constexpr auto typedCount = 12;
    auto keys = ChatKeyParser {};

    check(!keys.feed("1"));
    check(!keys.feed("2"));
    check(keys.getPendingKeys() == "12");

    auto command = keys.feed("k");
    check(command && command->action == ChatAction::up
          && command->count == typedCount);
    check(keys.getPendingKeys().empty());
};

auto tSequences = test("ChatKeymap/gg goes to the top and g? opens help") = []
{
    auto keys = ChatKeyParser {};

    check(!keys.feed("g"));
    check(keys.feed("g")->action == ChatAction::top);

    check(!keys.feed("g"));
    check(keys.feed("?")->action == ChatAction::help);
};

auto tBadSequence = test("ChatKeymap/an unknown sequence clears") = []
{
    auto keys = ChatKeyParser {};

    check(!keys.feed("g"));
    check(!keys.feed("z"));
    check(keys.getPendingKeys().empty());
    check(keys.feed("j")->action == ChatAction::down);
};

auto tEscape = test("ChatKeymap/Esc cancels a pending count") = []
{
    auto keys = ChatKeyParser {};

    keys.feed("3");
    check(keys.feed("Esc")->action == ChatAction::cancel);
    check(keys.getPendingKeys().empty());
};

auto tDiffViewKeys =
    test("ChatKeymap/dv's keys: panes, half pages, compose and quit") = []
{
    auto keys = ChatKeyParser {};

    check(keys.feed("C-d")->action == ChatAction::halfPageDown);
    check(keys.feed("C-u")->action == ChatAction::halfPageUp);
    check(keys.feed("h")->action == ChatAction::focusRooms);
    check(keys.feed("l")->action == ChatAction::focusMessages);
    check(keys.feed("Tab")->action == ChatAction::togglePane);
    check(keys.feed("c")->action == ChatAction::compose);
    check(keys.feed("i")->action == ChatAction::composeInInsertMode);
    check(keys.feed(":")->action == ChatAction::commandLine);
    check(keys.feed("q")->action == ChatAction::quit);
};

auto tNoRoomCycling = test("ChatKeymap/C-n and C-p do nothing") = []
{
    auto keys = ChatKeyParser {};

    check(!keys.feed("C-n"));
    check(!keys.feed("C-p"));
};
