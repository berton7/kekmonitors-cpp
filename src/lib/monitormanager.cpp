#include <iostream>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>

using namespace boost::asio;
using namespace boost::placeholders;
using namespace bsoncxx::builder::basic;

namespace kekmonitors {

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : _io(io), _config(std::move(config)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("MonitorManager");
    _kekDbConnection = std::make_unique<mongocxx::client>(mongocxx::uri{_config->parser.get<std::string>("GlobalConfig.db_path")});
    _kekDb = (*_kekDbConnection)[_config->parser.get<std::string>("GlobalConfig.db_name")];
    _monitorRegisterDb = _kekDb["register.monitors"];
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

    // various error checking

    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response);
        return;
    }

    std::string className;
    const auto &payload = cmd.getPayload();

    if (payload.find("name") != payload.end()) {
        className = payload.at("name");
    }
    else{
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"className\"");
        cb(response);
        return;
    }
    if (_monitorProcesses.find(className) != _monitorProcesses.end()) {
        response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
        response.setInfo("Monitor already started.");
        cb(response);
        return;
    }
    else if (_tmpMonitorProcesses.find(className) != _monitorProcesses.end())
    {
        response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
        response.setInfo("Monitor still being processed");
        cb(response);
        return;
    }
    const auto pythonExecutable = utils::getPythonExecutable();
    if (pythonExecutable.empty())
    {
        response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
        response.setInfo("Could not find a correct python version");
        cb(response);
        return;
    }

    // if we get here we can try to start the monitor

    const auto optRegisteredMonitor = _monitorRegisterDb.find_one( make_document(kvp("name", className)));

    if (!optRegisteredMonitor)
    {
         _logger->debug("Tried to add monitor {} but it was not registered", className);
	 Response r;
	 r.setError(ERRORS::MONITOR_NOT_REGISTERED);
	 cb(r);
	 return;
    }

    // insanity at its best!
    const auto path = optRegisteredMonitor.value().view()["path"].get_utf8().value.to_string();
    const auto monitorProcess = std::make_shared<MonitorProcess>(className, pythonExecutable, path, boost::process::std_out > boost::process::null, boost::process::std_err > boost::process::null);
    _tmpMonitorProcesses.insert({className, monitorProcess});

    auto delayTimer = std::make_shared<steady_timer>(_io, std::chrono::seconds(2));

    delayTimer->async_wait([this, delayTimer, monitorProcess, cb](const std::error_code &ec) {
        const auto &className = monitorProcess->getClassName();
        _tmpMonitorProcesses.erase(className);
        if (ec)
        {
            _logger->error(ec.message());
            auto response = Response::badResponse();
            const std::string msg{"Internal status timer failed with message: " + ec.message()};
            response.setInfo(msg);
            _logger->warn(msg);
            cb(response);
            return;
        }
        if (monitorProcess->getProcess().running()) {
            _monitorProcesses.insert({className, monitorProcess});
            _logger->info("Correctly added Monitor " + monitorProcess->getClassName());
            cb(Response::okResponse());
            return;
        }
        Response response;
        const std::string msg {"Monitor process exited too soon. Exit code: " + std::to_string(monitorProcess->getProcess().exit_code())};
        response.setError(ERRORS::MM_COULDNT_ADD_MONITOR);
        response.setInfo(msg);
        _logger->info(msg);
        cb(response);
    });
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
