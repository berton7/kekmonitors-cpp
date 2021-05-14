#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>
#include <iostream>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : _config(std::move(config)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("MonitorManager");
    _unixServer = std::make_unique<UnixServer>(
        io, "MonitorManager",
        CallbackMap{{{COMMANDS::PING, std::bind(&MonitorManager::onPing, this,
                                                std::placeholders::_1)},
                     {COMMANDS::MM_STOP_MONITOR_MANAGER,
                      std::bind(&MonitorManager::shutdown, this)}}},
        _config);
}

MonitorManager::~MonitorManager() = default;

Response MonitorManager::onPing(const Cmd &cmd) {
    _logger->info("onPing callback!");
    auto response = Response::okResponse();
    response.setInfo("Pong");
    return response;
}

Response MonitorManager::shutdown() {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _unixServer->shutdown();
    return Response::okResponse();
}
} // namespace kekmonitors
