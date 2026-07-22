#pragma once

#include <ctime>
#include <string>

// The per-platform surface: implemented once per platform, in
// Platform-macOS.mm and Platform-Windows.cpp, so shared code stays
// preprocessor-free.
namespace iac
{
std::string rawHostname();

bool stdinIsTty();

std::tm localTime(std::time_t seconds);
} // namespace iac
