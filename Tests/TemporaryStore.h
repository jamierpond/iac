#pragma once

#include <Room.h>

#include <filesystem>
#include <random>
#include <string>

namespace iac::testing
{
// A store directory of its own under the system temp folder, deleted when the test ends.
struct TemporaryStore
{
    TemporaryStore()
    {
        std::filesystem::create_directories(directory);
    }

    ~TemporaryStore()
    {
        auto error = std::error_code {};
        std::filesystem::remove_all(directory, error);
    }

    TemporaryStore(const TemporaryStore&) = delete;
    TemporaryStore& operator=(const TemporaryStore&) = delete;

    Room getRoom(const std::string& name = {}) const
    {
        return {{}, directory.string(), name};
    }

    std::filesystem::path directory = std::filesystem::temp_directory_path()
                                      / ("iac-test-" + std::to_string(std::random_device {}()));
};
} // namespace iac::testing
