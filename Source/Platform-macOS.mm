#include "Platform.h"

#include <unistd.h>

namespace iac
{
std::string rawHostname()
{
    char name[256] = {};
    return gethostname(name, sizeof(name) - 1) == 0 ? name : "";
}

bool stdinIsTty() { return isatty(STDIN_FILENO) != 0; }

std::tm localTime(std::time_t seconds)
{
    auto local = std::tm {};
    localtime_r(&seconds, &local);
    return local;
}
} // namespace iac
