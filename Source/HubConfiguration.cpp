#include "HubConfiguration.h"

#include <Miro/Reflect.h>

namespace iac
{
namespace
{
constexpr auto configurationDirectoryName = std::string_view {".config"};
constexpr auto configurationFileName = std::string_view {"iac.json"};

struct HubConfiguration
{
    std::optional<std::string> hub;

    MIRO_REFLECT(hub)
};

Miro::JSON parseConfigurationFile(const eacp::FilePath& path)
{
    try
    {
        auto json = Miro::Json::parse(eacp::Files::readFile(path));
        if (json.isObject())
            return json;
    }
    catch (const Miro::Json::ParseError&)
    {
    }
    throw ConfigurationFileError {path.str() + " is not a JSON object"};
}

std::string getHostWithoutUserOrDomain(const std::string& sshTarget)
{
    auto host = sshTarget;
    if (const auto at = host.find('@'); at != std::string::npos)
        host.erase(0, at + 1);
    if (const auto dot = host.find('.'); dot != std::string::npos)
        host.resize(dot);
    return host;
}
} // namespace

eacp::FilePath getConfigurationFilePath()
{
    return eacp::FilePath::homeDirectory() / configurationDirectoryName
           / configurationFileName;
}

std::optional<Room> readHubRoomFromConfigurationFile(const eacp::FilePath& path)
{
    if (!eacp::Files::exists(path))
        return std::nullopt;

    const auto configuration =
        Miro::createFromJSON<HubConfiguration>(parseConfigurationFile(path));
    if (!configuration.hub)
        return std::nullopt;
    if (configuration.hub->empty())
    {
        throw ConfigurationFileError {
            path.str() + ": \"hub\" must be a room spec such as \"user@host:\""};
    }
    return parseRoomSpecification(*configuration.hub);
}

bool isHubThisMachine(const Room& hub, const std::string& shortHostname)
{
    if (!hub.isRemote())
        return false;
    return eacp::Strings::equalsCaseInsensitive(
        getHostWithoutUserOrDomain(hub.sshTarget), shortHostname);
}
} // namespace iac
