#include <LineEdit.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace iac;

auto tLineEdit = test("LineEdit/edits one line") = []
{
    auto line = LineEdit {};

    check(line.feed("a"));
    check(line.feed("\xC3\xA9"));
    check(line.feed("b"));
    check(line.text()
          == "a\xC3\xA9"
             "b");
    check(line.feed("Left"));
    check(line.feed("Backspace"));
    check(line.text() == "ab" && line.cursor() == 1);
    check(!line.feed("Tab"));
    check(!line.feed("F1"));
    check(!line.feed("Enter"));

    line.set("one two three");
    check(line.feed("C-w"));
    check(line.text() == "one two ");
    check(line.feed("C-u"));
    check(line.text().empty());
};
