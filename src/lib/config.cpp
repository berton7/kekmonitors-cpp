#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <kekmonitors/config.hpp>
#include <kekmonitors/utils.hpp>

using namespace boost;
namespace pt = boost::property_tree;

namespace kekmonitors {

const std::string Config::s_defaultConfig = boost::str(
    boost::format(
        "[GlobalConfig]\n"
        "socket_path = %s/sockets\n"
        "log_path = %s/logs\n"
        "db_name = kekmonitors\n"
        "db_path = mongodb://localhost:27017/\n"
        "\n"
        "[WebhookConfig]\n"
        "crash_webhook = \n"
        "provider = KekMonitors\n"
        "provider_icon = "
        "https://avatars0.githubusercontent.com/u/"
        "11823129?s=400&u=3e617374871087e64b5fde0df668260f2671b076&v=4\n"
        "timestamp_format = %%d %%b %%Y, %%H:%%M:%%S.%%f\n"
        "embed_color = 255\n"
        "\n"
        "[OtherConfig]\n"
        "class_name =\n"
        "socket_name =\n"
        "\n"
        "[Options]\n"
        "add_stream_handler = True\n"
        "enable_config_watcher = True\n"
        "enable_webhooks = True\n"
        "loop_delay = 5\n"
        "max_last_seen = 2592000\n") %
    utils::getLocalKekDir() % utils::getLocalKekDir());

Config::Config() {
    std::string configPath = utils::getLocalKekDir() + "/config/config.cfg";
    utils::getContentIfFileExistsElseCreate(configPath, s_defaultConfig);
    pt::read_ini(configPath, p_parser);
}
Config::~Config() {}

Config &getConfig()
{
	static Config s_config;
    return s_config;
}
} // namespace kekmonitors
