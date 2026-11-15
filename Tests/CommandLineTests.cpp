// Runs the built iac binary against a temporary store, the way agents call it.
#include "TemporaryStore.h"

#include <eacp/Core/Process/Process.h>

#include <NanoTest/NanoTest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto publishRetryInterval = std::chrono::milliseconds {200};
constexpr auto monitorDeadline = std::chrono::seconds {20};

eacp::Processes::ProcessResult runIac(const testing::TemporaryStore& store,
                                      std::vector<std::string> arguments)
{
    auto options = eacp::Processes::ProcessOptions {};
    options.executable = IAC_BINARY_PATH;
    arguments.insert(arguments.end(), {"--room", store.directory.string()});
    for (auto& argument: arguments)
        options.arguments.push_back(std::move(argument));
    return eacp::Processes::run(std::move(options));
}

std::vector<std::string> splitLines(const std::string& text)
{
    auto lines = std::vector<std::string> {};
    auto line = std::string {};
    for (const auto character: text)
    {
        if (character == '\r')
            continue;
        if (character != '\n')
        {
            line += character;
            continue;
        }
        lines.push_back(line);
        line.clear();
    }
    return lines;
}
} // namespace

auto tPublishAndRead = test("CommandLine/publish then read prints the messages oldest first") = []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "first", "--from", "alice"}).exitCode == 0);
    check(runIac(store, {"publish", "second", "message", "--from", "bob", "--cwd", "/x"}).exitCode == 0);

    const auto read = runIac(store, {"read", "-n", "5"});
    check(read.exitCode == 0);
    const auto lines = splitLines(read.output);
    check(lines.size() == 2);
    check(lines[0].find("] alice (") != std::string::npos);
    check(lines[0].ends_with(": first"));
    check(lines[1].ends_with("] bob (/x): second message"));
};

auto tReadCount = test("CommandLine/read -n prints only the newest") = []
{
    const auto store = testing::TemporaryStore {};
    for (const auto* text: {"a", "b", "c"})
        check(runIac(store, {"publish", text, "--from", "x"}).exitCode == 0);
    const auto lines = splitLines(runIac(store, {"read", "-n", "1"}).output);
    check(lines.size() == 1);
    check(lines[0].ends_with(": c"));
};

auto tPublishWithoutText = test("CommandLine/publish with no text is a usage error") = []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "--from", "x"}).exitCode == 2);
};

auto tMonitorOnce = test("CommandLine/monitor --once prints the next message and exits, skipping its own") = []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "from before", "--from", "x"}).exitCode == 0);

    auto monitorResult = eacp::Processes::ProcessResult {};
    auto isMonitorDone = std::atomic<bool> {false};
    auto monitorThread = std::thread {[&]
                                      {
                                          monitorResult = runIac(store, {"monitor", "--once", "--ignore-from", "me"});
                                          isMonitorDone = true;
                                      }};

    // The monitor only sees messages published after it starts, so keep
    // publishing until it has printed one.
    const auto deadline = std::chrono::steady_clock::now() + monitorDeadline;
    while (!isMonitorDone && std::chrono::steady_clock::now() < deadline)
    {
        runIac(store, {"publish", "my own", "--from", "me"});
        runIac(store, {"publish", "wake up", "--from", "other"});
        std::this_thread::sleep_for(publishRetryInterval);
    }
    monitorThread.join();

    check(monitorResult.exitCode == 0);
    const auto lines = splitLines(monitorResult.output);
    check(lines.size() == 1);
    check(lines[0].find("] other (") != std::string::npos);
    check(lines[0].ends_with(": wake up"));
};
