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
        CallbackMap{{COMMANDS::PING, std::bind(&MonitorManager::onPing, this,
                                                std::placeholders::_1)},
                     {COMMANDS::MM_STOP_MONITOR_MANAGER,
                      std::bind(&MonitorManager::shutdown, this)},
                    {COMMANDS::MM_ADD_MONITOR, std::bind(&MonitorManager::onAddMonitor, this, std::placeholders::_1)},
                    {COMMANDS::MM_GET_MONITOR_STATUS, std::bind(&MonitorManager::onGetMonitorStatus, this, std::placeholders::_1)}},
        _config);
}

MonitorManager::~MonitorManager() = default;

Response MonitorManager::onPing(const Cmd &cmd) {
    _logger->info("onPing callback!");
    auto response = Response::okResponse();
    response.setInfo("Pong");
    return response;
}

Response MonitorManager::onAddMonitor(const Cmd &cmd){
    if (cmd.getPayload() == nullptr)
    {
        Response response;
        response.setError(ERRORS::MISSING_PAYLOAD);
        return response;
    }
    try {
        const auto name = cmd.getPayload().at("name");
        auto monitorProcess = std::make_unique<MonitorProcess>(name, utils::getPythonExecutable(), "");
        // TODO: wait to check if still alive
        _monitorProcesses.insert({name, std::move(monitorProcess)});
        return Response::okResponse();
    } catch (json::out_of_range &) {
        Response resp;
        resp.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        resp.setInfo("Missing payload arg: \"name\"");
        return resp;
    }
}

Response MonitorManager::onGetMonitorStatus(const Cmd &cmd)
{
    auto response = Response::okResponse();
    if (!_monitorProcesses.empty() /* && !_scraperProcesses.empty()*/)
    {
        json payload;
        if (!_monitorProcesses.empty()) {
            json monitoredProcesses;
            for (const auto &monitorProcess : _monitorProcesses) {
                monitoredProcesses.push_back(monitorProcess.second->toJson());
            }
            payload["monitored_processes"] = monitoredProcesses;
        }
        // TODO: add _scraperProcesses
        response.setPayload(payload);
    }
    return response;
}

Response MonitorManager::shutdown() {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _unixServer->shutdown();
    return Response::okResponse();
}
} // namespace kekmonitors
