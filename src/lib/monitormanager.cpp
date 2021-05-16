#include <iostream>
#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : _io(io), _config(std::move(config)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("MonitorManager");
    _unixServer = std::make_unique<UnixServer>(
        io, "MonitorManager",
        CallbackMap{
            {COMMANDS::PING,
             std::bind(&MonitorManager::onPing, this, std::placeholders::_1, std::placeholders::_2)},
            {COMMANDS::MM_STOP_MONITOR_MANAGER,
             std::bind(&MonitorManager::shutdown, this, std::placeholders::_1,
                       std::placeholders::_2)},
            {COMMANDS::MM_ADD_MONITOR, std::bind(&MonitorManager::onAddMonitor,
                                                 this, std::placeholders::_1, std::placeholders::_2)},
            {COMMANDS::MM_GET_MONITOR_STATUS,
             std::bind(&MonitorManager::onGetMonitorStatus, this,
                       std::placeholders::_1, std::placeholders::_2)}},
        _config);
}

MonitorManager::~MonitorManager() = default;

void MonitorManager::onPing(const Cmd &cmd, const ResponseCallback &&cb) {
    _logger->info("onPing callback!");
    auto response = Response::okResponse();
    response.setInfo("Pong");
    cb(response);
}

void MonitorManager::onAddMonitor(const Cmd &cmd, const ResponseCallback &&cb) {
    Response response;
    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response);
        return;
    }
    try {
        const auto name = cmd.getPayload().at("name");
        try {
            _monitorProcesses.at(name);
            response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
            response.setInfo("Monitor already started.");
            cb(response);
            return;
        } catch (std::out_of_range &) {
            auto pythonExecutable = utils::getPythonExecutable();
            if (pythonExecutable.empty())
            {
                response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
                response.setInfo("Could not find a correct python version");
                cb(response);
                return;
            }

            auto monitorProcess = std::make_shared<MonitorProcess>(name, pythonExecutable, "");

            auto delayTimer = std::make_shared<steady_timer>(_io, std::chrono::seconds(2));

            delayTimer->async_wait([=](const std::error_code &ec) {
                if (monitorProcess->getProcess().running()) {
                    cb(Response::okResponse());
                    _monitorProcesses.insert({name, monitorProcess});
                }
                else
                {
                    auto s = monitorProcess->getClassName();
                    auto i = _monitorProcesses.find(s);
                    Response response;
                    response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
                    response.setInfo("Monitor process exited too soon. Exit code: " + std::to_string(monitorProcess->getProcess().exit_code()));
                    cb(response);
                }
            });
            return;
        }
    } catch (json::out_of_range &) {
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"name\"");
        cb(response);
        return;
    }
}

void MonitorManager::onGetMonitorStatus(const Cmd &cmd,
                                            const ResponseCallback &&cb) {
    Response response;
    if (!_monitorProcesses.empty() /* && !_scraperProcesses.empty()*/) {
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
    cb(response);
}

void MonitorManager::shutdown(const Cmd &cmd, const ResponseCallback &&cb) {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _unixServer->shutdown();
    cb(Response::okResponse());
}
} // namespace kekmonitors
