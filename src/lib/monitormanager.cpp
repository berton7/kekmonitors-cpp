#include <iostream>
#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : _config(config) {
    if (config == nullptr)
        _config = std::make_shared<Config>();
    _unixServer = std::make_unique<UnixServer>(
        io, "MonitorManager", _config,
        CallbackMap({{COMMANDS::PING, &MonitorManager::onPing},
                     {COMMANDS::MM_STOP_MONITOR_MANAGER,
                      std::bind(&MonitorManager::shutdown, this)}}));
}

MonitorManager::~MonitorManager() = default;

Response MonitorManager::onPing(const Cmd &cmd) {
    std::cout << "onPing callback!" << std::endl;
    return Response::okResponse();
}

Response MonitorManager::shutdown() {
    KDBGD("Received shutdown");
    _unixServer->shutdown();
    return Response::okResponse();
}
} // namespace kekmonitors
