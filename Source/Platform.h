#pragma once

#include <ctime>
#include <string>
#include <vector>

// The per-platform surface: implemented once per platform, in
// Platform-macOS.mm and Platform-Windows.cpp, so shared code stays
// preprocessor-free.
namespace iac
{
std::string rawHostname();

// The arguments after the program name. Windows main() receives argv in the
// console codepage, so non-ASCII text arrives mangled; the wide command line
// is authoritative there and argc/argv are ignored.
std::vector<std::string> commandLineArguments(int argc, char* argv[]);

bool stdinIsTty();

std::tm localTime(std::time_t seconds);
} // namespace iac
