#include "Platform.h"

#include <cstdio>
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

int execute(std::vector<std::string> argv)
{
    auto arguments = std::vector<char*> {};
    for (auto& argument: argv)
        arguments.push_back(argument.data());
    arguments.push_back(nullptr);

    execvp(arguments.front(), arguments.data());
    std::perror(("iac: could not run " + argv.front()).c_str());
    return 127;
}
} // namespace iac
