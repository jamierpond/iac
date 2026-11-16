#include "MessageAddressing.h"

#include <eacp/Core/Utils/Format.h>
#include <eacp/Core/Utils/Strings.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace iac
{
namespace
{
constexpr auto mentionMarker = '@';
constexpr auto everyoneName = std::string_view {"all"};

bool isNameCharacter(char character)
{
    const auto byte = static_cast<unsigned char>(character);
    return std::isalnum(byte) != 0 || character == '-' || character == '_';
}

// Each '@' not preceded by a name character starts a mention; the name runs
// to the first character that is not a name character.
std::vector<std::string> getMentionedNames(const std::string& text)
{
    auto names = std::vector<std::string> {};
    for (auto marker = std::ranges::find(text, mentionMarker); marker != text.end();
         marker = std::find(std::next(marker), text.end(), mentionMarker))
    {
        if (marker != text.begin() && isNameCharacter(*std::prev(marker)))
            continue;
        const auto nameStart = std::next(marker);
        const auto nameEnd =
            std::find_if_not(nameStart, text.end(), isNameCharacter);
        names.emplace_back(nameStart, nameEnd);
    }
    return names;
}

std::string getMessagesNoun(const SkippedMessages& skipped)
{
    if (skipped.notAddressedCount == 1)
        return "message";
    return "messages";
}
} // namespace

bool isMessageAddressedTo(const std::string& text, const std::string& agentName)
{
    const auto names = getMentionedNames(text);
    return std::ranges::any_of(
        names,
        [&](const auto& name)
        {
            return eacp::Strings::equalsCaseInsensitive(name, agentName)
                   || eacp::Strings::equalsCaseInsensitive(name, everyoneName);
        });
}

std::string describeSkippedMessages(const SkippedMessages& skipped)
{
    return eacp::Strings::format(
        "iac: skipped {} {} not addressed to you, the first at {}; "
        "iac read -n {} shows them",
        skipped.notAddressedCount,
        getMessagesNoun(skipped),
        skipped.firstSkippedTimeOfDay,
        skipped.readCountThatShowsThem);
}
} // namespace iac
