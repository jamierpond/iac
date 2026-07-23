#include "Platform.h"

#include <cstdlib>
#include <cwchar>
#include <io.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

namespace iac
{
std::string rawHostname()
{
    const auto* name = std::getenv("COMPUTERNAME");
    return name != nullptr ? name : "";
}

std::vector<std::string> commandLineArguments(int, char*[])
{
    auto count = 0;
    auto** wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide == nullptr)
        return {};

    auto arguments = std::vector<std::string> {};
    for (auto i = 1; i < count; ++i)
    {
        const auto wideLength = (int) wcslen(wide[i]);
        const auto length = WideCharToMultiByte(
            CP_UTF8, 0, wide[i], wideLength, nullptr, 0, nullptr, nullptr);
        auto argument = std::string ((std::size_t) length, '\0');
        WideCharToMultiByte(CP_UTF8,
                            0,
                            wide[i],
                            wideLength,
                            argument.data(),
                            length,
                            nullptr,
                            nullptr);
        arguments.push_back(std::move(argument));
    }

    LocalFree(wide);
    return arguments;
}

bool stdinIsTty() { return _isatty(_fileno(stdin)) != 0; }

std::tm localTime(std::time_t seconds)
{
    auto local = std::tm {};
    localtime_s(&local, &seconds);
    return local;
}
} // namespace iac
