#include "Platform.h"

#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <process.h>

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

int execute(std::vector<std::string> argv)
{
    auto arguments = std::vector<char*> {};
    for (auto& argument: argv)
        arguments.push_back(argument.data());
    arguments.push_back(nullptr);

    const auto status = _spawnvp(_P_WAIT, arguments.front(), arguments.data());
    if (status < 0)
    {
        std::perror(("iac: could not run " + argv.front()).c_str());
        return 127;
    }
    return static_cast<int>(status);
}
} // namespace iac
