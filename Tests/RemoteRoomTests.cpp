#include <RemoteRoom.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>
#include <string>

using namespace nano;
using namespace iac;

namespace
{
constexpr auto hubSshTarget = "jamiepond@macprorack";
constexpr auto expectedConnectTimeoutOption = "ConnectTimeout=5";
constexpr auto localRoomArgument = "--room ~/.iac";

Room makeRoomOnHost(const std::string& sshTarget)
{
    return {sshTarget, {}, {}};
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}
} // namespace

auto tForwardedDefaultStore =
    test("RemoteRoom/a forwarded room always names its store") = []
{
    check(getForwardedRoomSpecification(makeRoomOnHost("macprorack")) == "~/.iac");
    check(getForwardedRoomSpecification({"macprorack", {}, "team"})
          == "~/.iac#team");
    check(getForwardedRoomSpecification({"macprorack", "/srv/chat", "team"})
          == "/srv/chat#team");
};

auto tConnectTimeout =
    test("RemoteRoom/ssh gives up connecting after five seconds") = []
{
    const auto options = getSshConnectionOptions();
    check(std::find(options.begin(), options.end(), expectedConnectTimeoutOption)
          != options.end());
};

auto tUnreachableHostMessage = test(
    "RemoteRoom/an unreachable host is named, with the way to the local room") = []
{
    const auto message = describeUnreachableHost(makeRoomOnHost(hubSshTarget));
    check(message.starts_with("iac: "));
    check(contains(message, hubSshTarget));
    check(contains(message, localRoomArgument));
    check(!contains(message, "\n"));
};
