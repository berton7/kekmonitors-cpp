#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>
#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>
#include <mongocxx/exception/query_exception.hpp>
#include <utility>

using namespace boost::asio;
using namespace boost::placeholders;
using namespace bsoncxx::builder::basic;

namespace kekmonitors {

MonitorScraperCompletion::MonitorScraperCompletion(
    io_service &io, MonitorManager *moman, Cmd cmd,
    MonitorManagerCallback &&momanCb, DoubleResponseCallback &&completionCb,
    std::shared_ptr<Connection> connection)
    : _io(io), _cmd(std::move(cmd)), _completionCb(std::move(completionCb)),
      _momanCb(std::move(momanCb)), _moman(moman),
      _connection(std::move(connection)) {
    KDBG("CTOR");
};

MonitorScraperCompletion::~MonitorScraperCompletion() { KDBG("dtor"); }

void MonitorScraperCompletion::run() {
    auto shared = shared_from_this();
    post(_io, [shared] {
        return shared->_momanCb(
            shared->_moman, MonitorOrScraper::Monitor, shared->_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      std::placeholders::_1));
    });
    post(_io, [shared] {
        return shared->_momanCb(
            shared->_moman, MonitorOrScraper::Scraper, shared->_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      std::placeholders::_1));
    });
};

void MonitorScraperCompletion::create(
    io_service &io, MonitorManager *moman, const Cmd &cmd,
    MonitorManagerCallback &&momanCb, DoubleResponseCallback &&completionCb,
    std::shared_ptr<Connection> connection) {
    std::make_shared<MonitorScraperCompletion>(
        io, moman, cmd, std::move(momanCb), std::move(completionCb),
        std::move(connection))
        ->run();
}

void MonitorScraperCompletion::checkForCompletion(const Response &response) {
    if (!_bothCompleted) {
        _bothCompleted = true;
        _firstResponse = response;
        return;
    }
    _completionCb(_firstResponse, response);
}

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : _io(io), _processCheckTimer(io, std::chrono::milliseconds(500)),
      _config(std::move(config)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("MonitorManager");
    _kekDbConnection = std::make_unique<mongocxx::client>(mongocxx::uri{
        _config->parser.get<std::string>("GlobalConfig.db_path")});
    _kekDb = (*_kekDbConnection)[_config->parser.get<std::string>(
        "GlobalConfig.db_name")];
    _monitorRegisterDb = _kekDb["register.monitors"];
    _scraperRegisterDb = _kekDb["register.scrapers"];
#ifdef KEKMONITORS_DEBUG
    KDBG("Removing leftover monitor manager socket. Beware this is a "
         "debug-only feature.");
    ::unlink(std::string{utils::getLocalKekDir() + "/sockets/MonitorManager"}
                 .c_str());
#endif
    _unixServer = std::make_unique<UnixServer>(
        io, "MonitorManager",
        CallbackMap{
            {COMMANDS::PING,
             std::bind(&MonitorManager::onPing, this, std::placeholders::_1,
                       std::placeholders::_2)},
            {COMMANDS::MM_STOP_MONITOR_MANAGER,
             std::bind(&MonitorManager::shutdown, this, std::placeholders::_1,
                       std::placeholders::_2)},
            {COMMANDS::MM_ADD_MONITOR,
             std::bind(&MonitorManager::onAdd, this, MonitorOrScraper::Monitor,
                       std::placeholders::_1, std::placeholders::_2)},
            {COMMANDS::MM_GET_MONITOR_STATUS,
             std::bind(&MonitorManager::onGetStatus, this,
                       MonitorOrScraper::Monitor, std::placeholders::_1,
                       std::placeholders::_2)},
            {COMMANDS::MM_GET_SCRAPER_STATUS,
             std::bind(&MonitorManager::onGetStatus, this,
                       MonitorOrScraper::Scraper, std::placeholders::_1,
                       std::placeholders::_2)},
            {COMMANDS::MM_GET_MONITOR_SCRAPER_STATUS,
             std::bind(&MonitorManager::onGetMonitorScraperStatus, this,
                       std::placeholders::_1, std::placeholders::_2,
                       std::placeholders::_3)},
            {COMMANDS::MM_ADD_MONITOR_SCRAPER,
             std::bind(&MonitorManager::onAddMonitorScraper, this,
                       std::placeholders::_1, std::placeholders::_2,
                       std::placeholders::_3)}},
        _config);
    _fileWatcher.watches.emplace_back(
        _config->parser.get<std::string>("GlobalConfig.socket_path"),
        IN_ALL_EVENTS);
    _fileWatcher.inotify.Add(_fileWatcher.watches[0]);
    _fileWatcher.watchThread = std::thread([&] {
        while (!_fileWatcherStop) {
            _fileWatcher.inotify.WaitForEvents();
            size_t count = _fileWatcher.inotify.GetEventCount();
            while (count-- > 0) {
                InotifyEvent event;
                bool got_event = _fileWatcher.inotify.GetEvent(&event);
                if (got_event) {
                    _fileWatcher.eventQueue.emplace_back(std::move(event));
                    _fileWatcherAddEvent = true;
                }
            }
        }
    });

    _processCheckTimer.async_wait(std::bind(&MonitorManager::checkProcesses,
                                            this, std::placeholders::_1));
}

MonitorManager::~MonitorManager() { _fileWatcher.watchThread.join(); }

void MonitorManager::checkProcesses(const error_code &ec) {
    if (ec) {
        return;
    }
    for (auto it = _monitorProcesses.begin(); it != _monitorProcesses.end();) {
        auto mProcess = *it;
        if (!mProcess.second->getProcess().running()) {
            _logger->warn("Monitor {} has exited with code {}", mProcess.first,
                          mProcess.second->getProcess().exit_code());
            it = _monitorProcesses.erase(it);
        } else
            ++it;
    }
    for (auto it = _scraperProcesses.begin(); it != _scraperProcesses.end();) {
        auto sProcess = *it;
        if (!sProcess.second->getProcess().running()) {
            _logger->warn("Scraper {} has exited with code {}", sProcess.first,
                          sProcess.second->getProcess().exit_code());
            it = _scraperProcesses.erase(it);
        } else
            ++it;
    }
    _processCheckTimer.expires_after(std::chrono::milliseconds(500));
    _processCheckTimer.async_wait(std::bind(&MonitorManager::checkProcesses,
                                            this, std::placeholders::_1));
}

void MonitorManager::shutdown(const Cmd &cmd, const UserResponseCallback &&cb) {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _processCheckTimer.cancel();
    _fileWatcherStop = true;
    _unixServer->shutdown();
    cb(Response::okResponse());
}

void MonitorManager::onPing(const Cmd &cmd, const UserResponseCallback &&cb) {
    _logger->info("onPing callback!");
    auto response = Response::okResponse();
    response.setInfo("Pong");
    cb(response);
}

void MonitorManager::onAdd(const MonitorOrScraper m, const Cmd &cmd,
                           const UserResponseCallback &&cb) {
    Response response;
    ERRORS genericError = m == MonitorOrScraper::Monitor
                              ? ERRORS::MM_COULDNT_ADD_MONITOR
                              : ERRORS::MM_COULDNT_ADD_SCRAPER;

    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response);
        return;
    }

    std::string className;
    const json &payload = cmd.getPayload();

    if (payload.find("name") != payload.end()) {
        className = payload.at("name");
    } else {
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"name\".");
        cb(response);
        return;
    }

    auto &processes =
        m == MonitorOrScraper::Monitor ? _monitorProcesses : _scraperProcesses;
    auto &tmpProcesses = m == MonitorOrScraper::Monitor ? _tmpMonitorProcesses
                                                        : _tmpScraperProcesses;

    if (processes.find(className) != processes.end()) {
        response.setError(genericError);
        response.setInfo(
            std::string{
                (m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper")} +
            " already started.");
        cb(response);
        return;
    }

    else if (tmpProcesses.find(className) != tmpProcesses.end()) {
        response.setError(genericError);
        response.setInfo(
            std::string{
                (m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper")} +
            " still being processed.");
        cb(response);
        return;
    }

    const auto pythonExecutable = utils::getPythonExecutable();
    if (pythonExecutable.empty()) {
        response.setError(genericError);
        response.setInfo("Could not find a correct python version.");
        cb(response);
        return;
    }

    auto &registerDb = m == MonitorOrScraper::Monitor ? _monitorRegisterDb
                                                      : _scraperRegisterDb;

    boost::optional<bsoncxx::document::value> optRegisteredMonitor;
    try {
        optRegisteredMonitor =
            registerDb.find_one(make_document(kvp("name", className)));
    } catch (const mongocxx::query_exception &e) {
        response.setError(genericError);
        response.setInfo(
            "Failed to query the database (is it up and running?)\n" +
            std::string{e.what()});
        cb(response);
        return;
    }

    if (!optRegisteredMonitor) {
        _logger->debug("Tried to add {} {} but it was not registered",
                       m == MonitorOrScraper::Monitor ? "monitor" : "scraper",
                       className);
        response.setError(m == MonitorOrScraper::Monitor
                              ? ERRORS::MONITOR_NOT_REGISTERED
                              : ERRORS::SCRAPER_NOT_REGISTERED);
        cb(response);
        return;
    }

    // insanity at its best!
    const auto path = optRegisteredMonitor.value()
                          .view()["path"]
                          .get_utf8()
                          .value.to_string();
    const auto process = std::make_shared<Process>(
        className, pythonExecutable, path,
        boost::process::std_out > boost::process::null,
        boost::process::std_err > boost::process::null);
    tmpProcesses.insert({className, process});

    auto delayTimer =
        std::make_shared<steady_timer>(_io, std::chrono::seconds(2));

    delayTimer->async_wait([this, m, genericError, delayTimer, process,
                            cb](const std::error_code &ec) {
        const std::string &className = process->getClassName();
        auto &tmpProcesses = m == MonitorOrScraper::Monitor
                                 ? _tmpMonitorProcesses
                                 : _tmpScraperProcesses;
        tmpProcesses.erase("");
        if (ec) {
            _logger->error(ec.message());
            Response response{Response::badResponse()};
            const std::string msg{"Internal timer failed with message: " +
                                  ec.message()};
            response.setInfo(msg);
            _logger->warn(msg);
            cb(response);
            return;
        }
        if (process->getProcess().running()) {
            auto &_processes = m == MonitorOrScraper::Monitor
                                   ? _monitorProcesses
                                   : _scraperProcesses;
            _processes.insert({className, process});
            _logger->info("Correctly added {} {}",
                          m == MonitorOrScraper::Monitor ? "monitor "
                                                         : "scraper ",
                          process->getClassName());
            cb(Response::okResponse());
            return;
        }
        Response response;
        const std::string msg{
            "Process exited too soon. Exit code: " +
            std::to_string(process->getProcess().exit_code()) + "."};
        response.setError(genericError);
        response.setInfo(msg);
        _logger->info(msg);
        cb(response);
    });
}

void MonitorManager::onAddMonitorScraper(
    const Cmd &cmd, const UserResponseCallback &&cb,
    std::shared_ptr<Connection> connection) {
    MonitorScraperCompletion::create(
        _io, this, cmd, &MonitorManager::onAdd,
        [cb](const Response &firstResponse, const Response &secondResponse) {
            cb(utils::makeCommonResponse(
                firstResponse, secondResponse,
                ERRORS::MM_COULDNT_ADD_MONITOR_SCRAPER));
        },
        std::move(connection));
}

void MonitorManager::onGetStatus(const MonitorOrScraper m, const Cmd &cmd,
                                 const UserResponseCallback &&cb) {
    Response response;
    std::unordered_map<std::string, std::shared_ptr<Process>> &processes =
        m == MonitorOrScraper::Monitor ? _monitorProcesses : _scraperProcesses;
    json payload;
    if (!processes.empty()) {
        json monitoredProcesses;
        for (const auto &process : processes) {
            monitoredProcesses.push_back(process.second->toJson());
        }
        payload["monitored_processes"] = monitoredProcesses;
    }
    response.setPayload(payload);
    cb(response);
}

void MonitorManager::onGetMonitorScraperStatus(
    const Cmd &cmd, const UserResponseCallback &&cb,
    std::shared_ptr<Connection> connection) {
    MonitorScraperCompletion::create(
        _io, this, cmd, &MonitorManager::onGetStatus,
        [cb](const Response &firstResponse, const Response &secondResponse) {
            Response response{
                utils::makeCommonResponse(firstResponse, secondResponse)};
            response.setPayload({{"monitors", firstResponse.getPayload()},
                                 {"scrapers", secondResponse.getPayload()}});
            cb(response);
        },
        std::move(connection));
}
void MonitorManager::onStop(MonitorOrScraper m, const Cmd &cmd,
                            const kekmonitors::UserResponseCallback &&cb) {
    Response response;

    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response);
        return;
    }

    std::string className;
    const json &payload = cmd.getPayload();

    if (payload.find("name") != payload.end()) {
        className = payload.at("name");
    } else {
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"name\".");
        cb(response);
        return;
    }

    auto &processes =
        m == MonitorOrScraper::Monitor ? _monitorProcesses : _scraperProcesses;

    if (processes.find(className) == processes.end()) {
        response.setError(ERRORS::MM_COULDNT_STOP_MONITOR);
        response.setInfo(std::string{m == MonitorOrScraper::Monitor
                                         ? "Monitor"
                                         : "Scraper"} +
                         " was never started.");
        cb(response);
        return;
    }
}

void MonitorManager::onStopMonitorScraper(
    const Cmd &cmd, const UserResponseCallback &&cb,
    std::shared_ptr<Connection> connection) {}
} // namespace kekmonitors
