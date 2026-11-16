#include "TemporaryStore.h"

#include <HubConfiguration.h>
#include <RemoteRoom.h>

#include <NanoTest/NanoTest.h>

#include <string>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto configurationFileName = "iac.json";
constexpr auto hubHostname = "macprorack";

eacp::FilePath writeConfigurationFile(const testing::TemporaryStore& store,
                                      const std::string& contents)
{
    const auto path = eacp::FilePath {store.directory} / configurationFileName;
    eacp::Files::writeFile(path, contents);
    return path;
}

bool doesReadingThrowConfigurationFileError(const eacp::FilePath& path)
{
    try
    {
        readHubRoomFromConfigurationFile(path);
    }
    catch (const ConfigurationFileError&)
    {
        return true;
    }
    return false;
}

Room makeRoomOnHost(const std::string& sshTarget)
{
    return {sshTarget, {}, {}};
}
} // namespace

auto tHubFromFile = test("HubConfiguration/a hub key names the room") = []
{
    const auto store = testing::TemporaryStore {};
    const auto path = writeConfigurationFile(
        store, R"({"hub": "jamiepond@macprorack:~/chat#team"})");
    const auto hub = readHubRoomFromConfigurationFile(path);
    check(hub.has_value());
    check(hub->sshTarget == "jamiepond@macprorack");
    check(hub->directory == "~/chat");
    check(hub->name == "team");
};

auto tMissingFile = test("HubConfiguration/a missing file means no hub") = []
{
    const auto store = testing::TemporaryStore {};
    const auto path = eacp::FilePath {store.directory} / configurationFileName;
    check(!readHubRoomFromConfigurationFile(path).has_value());
};

auto tNoHubKey = test("HubConfiguration/a file without a hub key means no hub") = []
{
    const auto store = testing::TemporaryStore {};
    const auto path = writeConfigurationFile(store, R"({"other": 1})");
    check(!readHubRoomFromConfigurationFile(path).has_value());
};

auto tMalformed = test("HubConfiguration/malformed JSON is an error") = []
{
    const auto store = testing::TemporaryStore {};
    check(doesReadingThrowConfigurationFileError(
        writeConfigurationFile(store, R"({"hub": )")));
};

auto tNotAnObject =
    test("HubConfiguration/a file that is not a JSON object is an error") = []
{
    const auto store = testing::TemporaryStore {};
    check(doesReadingThrowConfigurationFileError(
        writeConfigurationFile(store, R"(["jamiepond@macprorack:"])")));
};

auto tHubNotAString =
    test("HubConfiguration/a hub that is not a non-empty string is an error") = []
{
    const auto store = testing::TemporaryStore {};
    check(doesReadingThrowConfigurationFileError(
        writeConfigurationFile(store, R"({"hub": 5})")));
    check(doesReadingThrowConfigurationFileError(
        writeConfigurationFile(store, R"({"hub": ""})")));
};

auto tHubIsThisMachine = test(
    "HubConfiguration/the hub is this machine when its host matches the hostname") =
    []
{
    check(isHubThisMachine(makeRoomOnHost("jamiepond@macprorack"), hubHostname));
    check(isHubThisMachine(makeRoomOnHost("macprorack"), hubHostname));
    check(isHubThisMachine(makeRoomOnHost("jamiepond@MacProRack"), hubHostname));
    check(
        isHubThisMachine(makeRoomOnHost("macprorack.tail1234.ts.net"), hubHostname));
};

auto tHubIsAnotherMachine = test(
    "HubConfiguration/the hub is not this machine when the host differs or is local") =
    []
{
    check(!isHubThisMachine(makeRoomOnHost("jamiepond@daily-driver"), hubHostname));
    check(!isHubThisMachine(makeRoomOnHost("macprorack2"), hubHostname));
    check(!isHubThisMachine(Room {}, hubHostname));
};

auto tForwardedDefaultStore =
    test("RemoteRoom/a forwarded room always names its store") = []
{
    check(getForwardedRoomSpecification(makeRoomOnHost("macprorack")) == "~/.iac");
    check(getForwardedRoomSpecification({"macprorack", {}, "team"})
          == "~/.iac#team");
    check(getForwardedRoomSpecification({"macprorack", "/srv/chat", "team"})
          == "/srv/chat#team");
};
