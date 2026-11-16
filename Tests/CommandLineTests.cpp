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

// IAC_NAME is emptied so the name from the developer's own shell cannot filter
// what a test's monitor prints.
eacp::Processes::ProcessResult runIacInRoom(const std::string& roomSpecification,
                                            std::vector<std::string> arguments)
{
    auto options = eacp::Processes::ProcessOptions {};
    options.executable = IAC_BINARY_PATH;
    options.environment = {{"IAC_NAME", ""}};
    arguments.insert(arguments.end(), {"--room", roomSpecification});
    for (auto& argument: arguments)
        options.arguments.push_back(std::move(argument));
    return eacp::Processes::run(std::move(options));
}

eacp::Processes::ProcessResult runIac(const testing::TemporaryStore& store,
                                      std::vector<std::string> arguments)
{
    return runIacInRoom(store.directory.string(), std::move(arguments));
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

struct Publication
{
    std::string text;
    std::string sender;
};

// The monitor only sees messages published after it starts, so the
// publications are repeated, in order, until it exits.
std::vector<std::string>
    runMonitorWhilePublishing(const std::string& roomSpecification,
                              const std::vector<std::string>& monitorArguments,
                              const std::vector<Publication>& publications)
{
    auto monitorResult = eacp::Processes::ProcessResult {};
    auto isMonitorDone = std::atomic<bool> {false};
    auto monitorThread = std::thread {
        [&]
        {
            monitorResult = runIacInRoom(roomSpecification, monitorArguments);
            isMonitorDone = true;
        }};

    const auto deadline = std::chrono::steady_clock::now() + monitorDeadline;
    while (!isMonitorDone && std::chrono::steady_clock::now() < deadline)
    {
        for (const auto& publication: publications)
            runIacInRoom(
                roomSpecification,
                {"publish", publication.text, "--from", publication.sender});
        std::this_thread::sleep_for(publishRetryInterval);
    }
    monitorThread.join();

    check(monitorResult.exitCode == 0);
    return splitLines(monitorResult.output);
}

bool hasLineEndingWith(const std::vector<std::string>& lines,
                       const std::string& ending)
{
    for (const auto& line: lines)
    {
        if (line.ends_with(ending))
            return true;
    }
    return false;
}
} // namespace

auto tPublishAndRead =
    test("CommandLine/publish then read prints the messages oldest first") = []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "first", "--from", "alice"}).exitCode == 0);
    check(runIac(store,
                 {"publish", "second", "message", "--from", "bob", "--cwd", "/x"})
              .exitCode
          == 0);

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

auto tPublishWithoutText =
    test("CommandLine/publish with no text is a usage error") = []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "--from", "x"}).exitCode == 2);
};

auto tMonitorOnce = test(
    "CommandLine/monitor --once prints the next message addressed to it and exits, skipping its own") =
    []
{
    const auto store = testing::TemporaryStore {};
    check(runIac(store, {"publish", "from before", "--from", "x"}).exitCode == 0);

    const auto lines =
        runMonitorWhilePublishing(store.directory.string(),
                                  {"monitor", "--once", "--ignore-from", "me"},
                                  {{"my own", "me"}, {"@me wake up", "other"}});

    check(lines.size() == 1);
    check(lines[0].find("] other (") != std::string::npos);
    check(lines[0].ends_with(": @me wake up"));
};

auto tMonitorOnceWaitsForAddressed = test(
    "CommandLine/monitor --once does not exit on a message not addressed to it") = []
{
    const auto store = testing::TemporaryStore {};
    const auto lines =
        runMonitorWhilePublishing(store.directory.string(),
                                  {"monitor", "--once", "--ignore-from", "me"},
                                  {{"chatter", "other"}, {"@me now", "other"}});
    check(!lines.empty());
    check(lines.back().ends_with(": @me now"));
    check(!hasLineEndingWith(lines, ": chatter"));
};

auto tMonitorSkippedLine = test(
    "CommandLine/monitor says how many messages it skipped and which read shows them") =
    []
{
    const auto store = testing::TemporaryStore {};
    constexpr auto addressedMessagesToPrint = "2";
    const auto lines = runMonitorWhilePublishing(
        store.directory.string(),
        {"monitor", "-n", addressedMessagesToPrint, "--ignore-from", "me"},
        {{"chatter", "other"}, {"my own", "me"}, {"@ME: ping", "other"}});

    constexpr auto linesForSkippedAndAddressed = std::size_t {2};
    check(lines.size() >= linesForSkippedAndAddressed);
    const auto& skippedLine = lines[lines.size() - linesForSkippedAndAddressed];
    check(skippedLine.starts_with(
        "iac: skipped 1 message not addressed to you, the first at "));
    check(skippedLine.ends_with("; iac read -n 3 shows them"));
    check(lines.back().ends_with(": @ME: ping"));
    check(!hasLineEndingWith(lines, ": chatter"));
};

auto tMonitorAll =
    test("CommandLine/monitor --all prints messages not addressed to it") = []
{
    const auto store = testing::TemporaryStore {};
    const auto lines = runMonitorWhilePublishing(
        store.directory.string(),
        {"monitor", "--once", "--all", "--ignore-from", "me"},
        {{"chatter", "other"}});
    check(lines.size() == 1);
    check(lines[0].ends_with(": chatter"));
};

auto tMonitorUnnamed =
    test("CommandLine/monitor with no name prints every message") = []
{
    const auto store = testing::TemporaryStore {};
    const auto lines = runMonitorWhilePublishing(
        store.directory.string(), {"monitor", "--once"}, {{"chatter", "other"}});
    check(lines.size() == 1);
    check(lines[0].ends_with(": chatter"));
};

auto tMonitorBreakout =
    test("CommandLine/monitor in a named room prints every message") = []
{
    const auto store = testing::TemporaryStore {};
    const auto lines =
        runMonitorWhilePublishing(store.directory.string() + "#design",
                                  {"monitor", "--once", "--ignore-from", "me"},
                                  {{"chatter", "other"}});
    check(lines.size() == 1);
    check(lines[0].ends_with(": chatter"));
};
