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

bool stdinIsTty();

std::tm localTime(std::time_t seconds);

// Hand the process over to argv — exec on POSIX, spawn-and-wait on Windows —
// returning the command's exit code, or 127 if it could not run.
int execute(std::vector<std::string> argv);
} // namespace iac
