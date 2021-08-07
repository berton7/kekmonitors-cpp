#include "moman.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>
#include <kekmonitors/utils.hpp>
#include <mongocxx/exception/query_exception.hpp>
#include <mutex>
#include <stdexcept>
#include <utility>

#define REGISTER_CALLBACK(cmd, cb)                                             \
    { cmd, std::bind(cb, this, ph::_1, ph::_2, ph::_3) }

#define M_REGISTER_CALLBACK(cmd, cb)                                           \
    {                                                                          \
        cmd, std::bind(cb, this, MonitorOrScraper::Monitor, ph::_1, ph::_2,    \
                       ph::_3)                                                 \
    }

#define S_REGISTER_CALLBACK(cmd, cb)                                           \
    {                                                                          \
        cmd, std::bind(cb, this, MonitorOrScraper::Scraper, ph::_1, ph::_2,    \
                       ph::_3)                                                 \
    }

using namespace boost::asio;

static inline bool is_socket(const std::string &path) {
    struct stat s;
    stat(path.c_str(), &s);
    return S_ISSOCK(s.st_mode);
}

namespace kekmonitors {

MonitorScraperCompletion::MonitorScraperCompletion(
    io_service &io, MonitorManager *moman, Cmd cmd,
    MonitorManagerCallback &&momanCb, DoubleResponseCallback &&completionCb,
    Connection::Ptr connection)
    : _io(io), _cmd(std::move(cmd)), _completionCb(std::move(completionCb)),
      _momanCb(std::move(momanCb)), _moman(moman),
      _connection(std::move(connection)) {
    KDBG("CTOR");
};

MonitorScraperCompletion::~MonitorScraperCompletion() { KDBG("dtor"); }

void MonitorScraperCompletion::run() {
    auto shared = shared_from_this();
    post(_io, [=] {
        return shared->_momanCb(
            shared->_moman, MonitorOrScraper::Monitor, shared->_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      ph::_1),
            _connection);
    });
    post(_io, [=] {
        return shared->_momanCb(
            shared->_moman, MonitorOrScraper::Scraper, shared->_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      ph::_1),
            _connection);
    });
};

void MonitorScraperCompletion::create(io_service &io, MonitorManager *moman,
                                      const Cmd &cmd,
                                      MonitorManagerCallback &&momanCb,
                                      DoubleResponseCallback &&completionCb,
                                      Connection::Ptr connection) {
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
            REGISTER_CALLBACK(COMMANDS::PING, &MonitorManager::onPing),
            REGISTER_CALLBACK(COMMANDS::MM_STOP_MONITOR_MANAGER,
                              &MonitorManager::shutdown),
            M_REGISTER_CALLBACK(COMMANDS::MM_ADD_MONITOR,
                                &MonitorManager::onAdd),
            S_REGISTER_CALLBACK(COMMANDS::MM_ADD_SCRAPER,
                                &MonitorManager::onAdd),
            REGISTER_CALLBACK(COMMANDS::MM_ADD_MONITOR_SCRAPER,
                              &MonitorManager::onAddMonitorScraper),
            M_REGISTER_CALLBACK(COMMANDS::MM_GET_MONITOR_STATUS,
                                &MonitorManager::onGetStatus),
            S_REGISTER_CALLBACK(COMMANDS::MM_GET_SCRAPER_STATUS,
                                &MonitorManager::onGetStatus),
            REGISTER_CALLBACK(COMMANDS::MM_GET_MONITOR_SCRAPER_STATUS,
                              &MonitorManager::onGetMonitorScraperStatus),
            M_REGISTER_CALLBACK(COMMANDS::MM_STOP_MONITOR,
                                &MonitorManager::onStop),
            S_REGISTER_CALLBACK(COMMANDS::MM_STOP_SCRAPER,
                                &MonitorManager::onStop),
            REGISTER_CALLBACK(COMMANDS::MM_STOP_MONITOR_SCRAPER,
                              &MonitorManager::onStopMonitorScraper)},
        _config);
    _fileWatcher.watches.emplace_back(
        _config->parser.get<std::string>("GlobalConfig.socket_path"),
        IN_ALL_EVENTS);
    _fileWatcher.inotify.Add(_fileWatcher.watches[0]);
    _fileWatcher.watchThread = std::thread([&] {
        while (!_fileWatcherStop) {
            _fileWatcher.inotify.WaitForEvents();
            size_t count = _fileWatcher.inotify.GetEventCount();
            for (; count > 0; --count) {
                InotifyEvent event;
                bool got_event = _fileWatcher.inotify.GetEvent(&event);
                const auto &socketName = event.GetName();
                std::string eventType;
                event.DumpTypes(eventType);
                const auto socketFullPath = event.GetWatch()->GetPath() +
                                            fs::path::preferred_separator +
                                            socketName;
                if (got_event &&
                    (eventType == "IN_CREATE" && is_socket(socketFullPath) ||
                     eventType == "IN_DELETE")) {
                    for (const auto &type :
                         {std::string{"Monitor."}, std::string{"Scraper."}}) {
                        const auto typeLen = type.length();
                        if (socketName.substr(0, typeLen) == type) {
                            auto &map = type == "Monitor." ? _monitorSockets
                                                           : _scraperSockets;
                            std::string className{socketName.substr(
                                typeLen, socketName.length() - typeLen)};
                            if (eventType == "IN_CREATE") {
                                std::lock_guard lock(_socketLock);
                                KDBG("Socket was created, className: " +
                                     className);
                                map.emplace(std::make_pair(
                                    className, local::stream_protocol::endpoint(
                                                   socketFullPath)));
                            } else if (map.find(className) != map.end()) {
                                std::lock_guard lock(_socketLock);
                                KDBG("Socket was destroyed, className: " +
                                     className);
                                map.erase(className);
                            }
                            break;
                        }
                    }
                }
            }
        }
    });

    _processCheckTimer.async_wait(
        std::bind(&MonitorManager::checkProcesses, this, ph::_1));
}

MonitorManager::~MonitorManager() {
    for (auto &processes : {_monitorProcesses, _scraperProcesses})
        for (auto &process : _monitorProcesses)
            if (process.second->getProcess().running())
                process.second->getProcess().terminate();
    _fileWatcher.watchThread.join();
}

void MonitorManager::checkProcesses(const error_code &ec) {
    if (ec) {
        return;
    }
    for (auto it = _monitorProcesses.begin(); it != _monitorProcesses.end();) {
        auto mProcess = *it;
        auto &process = mProcess.second->getProcess();
        if (!process.running()) {
            _logger->warn("Monitor {} has exited with code {}", mProcess.first,
                          process.exit_code());
            // process.join();
            it = _monitorProcesses.erase(it);
        } else
            ++it;
    }
    for (auto it = _scraperProcesses.begin(); it != _scraperProcesses.end();) {
        auto sProcess = *it;
        auto &process = sProcess.second->getProcess();
        if (!process.running()) {
            _logger->warn("Scraper {} has exited with code {}", sProcess.first,
                          process.exit_code());
            // process.join();
            it = _scraperProcesses.erase(it);
        } else
            ++it;
    }
    _processCheckTimer.expires_after(std::chrono::milliseconds(500));
    _processCheckTimer.async_wait(
        std::bind(&MonitorManager::checkProcesses, this, ph::_1));
}

void MonitorManager::shutdown(const Cmd &cmd, const UserResponseCallback &&cb,
                              Connection::Ptr connection) {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _processCheckTimer.cancel();
    _fileWatcherStop = true;
    _unixServer->shutdown();
    cb(Response::okResponse(), connection);
}

void MonitorManager::onPing(const Cmd &cmd, const UserResponseCallback &&cb,
                            Connection::Ptr connection) {
    _logger->info("onPing callback!");
    auto response = Response::okResponse();
    response.setInfo("Pong");
    cb(response, connection);
}

void MonitorManager::onAdd(const MonitorOrScraper m, const Cmd &cmd,
                           const UserResponseCallback &&cb,
                           Connection::Ptr connection) {
    Response response;
    ERRORS genericError = m == MonitorOrScraper::Monitor
                              ? ERRORS::MM_COULDNT_ADD_MONITOR
                              : ERRORS::MM_COULDNT_ADD_SCRAPER;

    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response, connection);
        return;
    }

    std::string className;
    const json &payload = cmd.getPayload();

    if (payload.find("name") != payload.end()) {
        className = payload.at("name");
    } else {
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"name\".");
        cb(response, connection);
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
        cb(response, connection);
        return;
    }

    else if (tmpProcesses.find(className) != tmpProcesses.end()) {
        response.setError(genericError);
        response.setInfo(
            std::string{
                (m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper")} +
            " still being processed.");
        cb(response, connection);
        return;
    }

    const auto pythonExecutable = utils::getPythonExecutable().generic_string();
    if (pythonExecutable.empty()) {
        response.setError(genericError);
        response.setInfo("Could not find a correct python version.");
        cb(response, connection);
        return;
    }

    auto &registerDb = m == MonitorOrScraper::Monitor ? _monitorRegisterDb
                                                      : _scraperRegisterDb;

    bsoncxx::stdx::optional<bsoncxx::document::value> optRegisteredMonitor;
    try {
        optRegisteredMonitor =
            registerDb.find_one(bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp("name", className)));
    } catch (const mongocxx::query_exception &e) {
        response.setError(genericError);
        response.setInfo(
            "Failed to query the database (is it up and running?)\n" +
            std::string{e.what()});
        cb(response, connection);
        return;
    }

    if (!optRegisteredMonitor) {
        _logger->debug("Tried to add {} {} but it was not registered",
                       m == MonitorOrScraper::Monitor ? "monitor" : "scraper",
                       className);
        response.setError(m == MonitorOrScraper::Monitor
                              ? ERRORS::MONITOR_NOT_REGISTERED
                              : ERRORS::SCRAPER_NOT_REGISTERED);
        cb(response, connection);
        return;
    }

    // insanity at its best!
    const auto path = std::string{
        optRegisteredMonitor.value().view()["path"].get_utf8().value};
    const auto process = std::make_shared<Process>(
        className, pythonExecutable + " " + path,
        boost::process::std_out > boost::process::null,
        boost::process::std_err > boost::process::null);
    tmpProcesses.insert({className, process});

    auto delayTimer =
        std::make_shared<steady_timer>(_io, std::chrono::seconds(2));

    delayTimer->async_wait([=](const std::error_code &ec) {
        delayTimer.get(); // capture delayTimer
        const std::string &className = process->getClassName();
        auto &tmpProcesses = m == MonitorOrScraper::Monitor
                                 ? _tmpMonitorProcesses
                                 : _tmpScraperProcesses;
        tmpProcesses.erase(className);
        if (ec) {
            _logger->error(ec.message());
            Response response{Response::badResponse()};
            const std::string msg{"Internal timer failed with message: " +
                                  ec.message()};
            response.setInfo(msg);
            _logger->warn(msg);
            cb(response, connection);
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
            cb(Response::okResponse(), connection);
            return;
        }
        Response response;
        const std::string msg{
            "Process exited too soon. Exit code: " +
            std::to_string(process->getProcess().exit_code()) + "."};
        response.setError(genericError);
        response.setInfo(msg);
        _logger->info(msg);
        cb(response, connection);
    });
}

void MonitorManager::onAddMonitorScraper(const Cmd &cmd,
                                         const UserResponseCallback &&cb,
                                         Connection::Ptr connection) {
    MonitorScraperCompletion::create(
        _io, this, cmd, &MonitorManager::onAdd,
        [=](const Response &firstResponse, const Response &secondResponse) {
            cb(utils::makeCommonResponse(
                   firstResponse, secondResponse,
                   ERRORS::MM_COULDNT_ADD_MONITOR_SCRAPER),
               connection);
        },
        connection);
}

void MonitorManager::onGetStatus(const MonitorOrScraper m, const Cmd &cmd,
                                 const UserResponseCallback &&cb,
                                 Connection::Ptr connection) {
    Response response;
    json payload;
    json monitoredProcesses = json::object();
    json monitoredSockets = json::object();

    auto &processes =
        m == MonitorOrScraper::Monitor ? _monitorProcesses : _scraperProcesses;
    for (const auto &process : processes) {
        monitoredProcesses[process.first] = process.second->toJson();
    }
    payload["monitored_processes"] = monitoredProcesses;

    auto &sockets =
        m == MonitorOrScraper::Monitor ? _monitorSockets : _scraperSockets;
    if (!sockets.empty()) {
        for (const auto &socket : sockets) {
            monitoredSockets[socket.first] = socket.second.path();
        }
    }
    payload["monitored_sockets"] = monitoredSockets;

    response.setPayload(payload);
    cb(response, connection);
}

void MonitorManager::onGetMonitorScraperStatus(const Cmd &cmd,
                                               const UserResponseCallback &&cb,
                                               Connection::Ptr connection) {
    MonitorScraperCompletion::create(
        _io, this, cmd, &MonitorManager::onGetStatus,
        [=](const Response &firstResponse, const Response &secondResponse) {
            Response response{
                utils::makeCommonResponse(firstResponse, secondResponse)};
            json payload;
            json tmpPayload = firstResponse.getPayload();
            payload["monitors"] =
                tmpPayload.is_null() ? json::object() : tmpPayload;
            tmpPayload = secondResponse.getPayload();
            payload["scrapers"] =
                tmpPayload.is_null() ? json::object() : tmpPayload;
            response.setPayload(payload);
            cb(response, connection);
        },
        connection);
}
void MonitorManager::onStop(MonitorOrScraper m, const Cmd &cmd,
                            const kekmonitors::UserResponseCallback &&cb,
                            Connection::Ptr connection) {
    Response response;

    if (cmd.getPayload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response, connection);
        return;
    }

    std::string className;
    const json &payload = cmd.getPayload();

    if (payload.find("name") != payload.end()) {
        className = payload.at("name");
    } else {
        response.setError(ERRORS::MISSING_PAYLOAD_ARGS);
        response.setInfo("Missing payload arg: \"name\".");
        cb(response, connection);
        return;
    }

    auto &sockets =
        m == MonitorOrScraper::Monitor ? _monitorSockets : _scraperSockets;

    if (sockets.find(className) == sockets.end()) {
        response.setError(ERRORS::SOCKET_DOESNT_EXIST);
        response.setInfo(std::string{m == MonitorOrScraper::Monitor
                                         ? "Monitor "
                                         : "Scraper "} +
                         className + " not present in available sockets.");
        cb(response, connection);
        return;
    }

    auto &ep = sockets.at(className);
    auto newConn = Connection::create(_io);
    newConn->socket.connect(ep);
    Cmd newCmd;
    newCmd.setCmd(COMMANDS::STOP);
    newConn->asyncWriteCmd(
        newCmd,
        [this, className, cb, m](const error_code &errc, Connection::Ptr conn) {
            if (!errc) {
                KDBG("Sent STOP");
                conn->asyncReadResponse(
                    [this, className, cb, m](const error_code &errc,
                                             const Response &response,
                                             Connection::Ptr conn) {
                        if (errc)
                            KDBG(errc.message());
                        cb(response, conn);
                    });
            } else {
                KDBG(errc.message());
                Response response;
                response.setError(m == MonitorOrScraper::Monitor
                                      ? ERRORS::MM_COULDNT_STOP_MONITOR
                                      : MM_COULDNT_STOP_SCRAPER);
                response.setInfo("Failed to send the STOP command. Errc: " +
                                 errc.message());
                cb(response, conn);
            }
        });
}

void MonitorManager::onStopMonitorScraper(const Cmd &cmd,
                                          const UserResponseCallback &&cb,
                                          Connection::Ptr connection) {
    MonitorScraperCompletion::create(
        _io, this, cmd, &MonitorManager::onStop,
        [=](const Response &firstResponse, const Response &secondResponse) {
            cb(utils::makeCommonResponse(
                   firstResponse, secondResponse,
                   ERRORS::MM_COULDNT_STOP_MONITOR_SCRAPER),
               connection);
        },
        connection);
}
} // namespace kekmonitors

int main() {
    io_context io;
    kekmonitors::init();
    kekmonitors::MonitorManager moman(io);
    io.run();
    return 0;
}
