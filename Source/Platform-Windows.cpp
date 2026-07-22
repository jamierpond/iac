#include "Platform.h"

#include <cstdlib>
#include <io.h>

namespace iac
{
std::string rawHostname()
{
    const auto* name = std::getenv("COMPUTERNAME");
    return name != nullptr ? name : "";
}

bool stdinIsTty() { return _isatty(_fileno(stdin)) != 0; }

std::tm localTime(std::time_t seconds)
{
    auto local = std::tm {};
    localtime_s(&local, &seconds);
    return local;
}
} // namespace iac
