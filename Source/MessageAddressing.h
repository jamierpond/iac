#pragma once

#include <cstddef>
#include <string>

namespace iac
{
// True when the text mentions '@<agentName>' or '@all', ignoring case. A
// mention is an '@' not preceded by a letter, digit, '-' or '_', followed by
// the longest run of those characters, so '@hub' does not match '@hub-2'.
bool isMessageAddressedTo(const std::string& text, const std::string& agentName);

struct SkippedMessages
{
    std::size_t notAddressedCount = 0;
    std::string firstSkippedTimeOfDay;
    // Every message since the last one printed, the one printed now and own
    // messages included, so 'iac read -n' with it shows all the skipped ones.
    std::size_t readCountThatShowsThem = 0;
};

// "iac: skipped 14 messages not addressed to you, the first at 03:12:05;
// iac read -n 15 shows them", no newline.
std::string describeSkippedMessages(const SkippedMessages& skipped);
} // namespace iac
