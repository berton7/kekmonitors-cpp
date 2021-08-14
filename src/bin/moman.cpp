#include "moman.hpp"
#include "kekmonitors/core.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_category.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>
#include <kekmonitors/utils.hpp>
#include <memory>
#include <mongocxx/exception/query_exception.hpp>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

template <typename Map, typename Iterator>
void removeStoredSocket(Map &map, Iterator &it) {
    auto &stored = it->second;
    stored.p_socket = nullptr;
    if (!stored.p_process)
        it = map.erase(it);
}

template <typename Map, typename Iterator>
void removeStoredProcess(Map &map, Iterator &it) {
    auto &stored = it->second;
    stored.p_process = nullptr;
    if (!stored.p_socket)
        it = map.erase(it);
}

MonitorScraperCompletion::MonitorScraperCompletion(
    io_context &io, MonitorManager *moman, Cmd cmd,
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

void MonitorScraperCompletion::create(io_context &io, MonitorManager *moman,
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
    : _io(io), _fileWatcher(io),
      _processCheckTimer(io, std::chrono::milliseconds(500)),
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
    _fileWatcher.inotify.AsyncStartWaitForEvents(
        std::bind(&MonitorManager::onInotifyUpdate, this));

    _processCheckTimer.async_wait(
        std::bind(&MonitorManager::checkProcesses, this, ph::_1));
}

void MonitorManager::onInotifyUpdate() {
    size_t count = _fileWatcher.inotify.GetEventCount();
    for (; count > 0; --count) {
        InotifyEvent event;
        bool got_event = _fileWatcher.inotify.GetEvent(&event);
        const auto &socketName = event.GetName();
        std::string eventType;
        event.DumpTypes(eventType);
        const auto socketFullPath = event.GetWatch()->GetPath() +
                                    fs::path::preferred_separator + socketName;
        if (got_event &&
            (eventType == "IN_CREATE" && is_socket(socketFullPath) ||
             eventType == "IN_DELETE")) {
            updateSockets(MonitorOrScraper::Monitor, eventType, socketName,
                          socketFullPath);
            updateSockets(MonitorOrScraper::Scraper, eventType, socketName,
                          socketFullPath);
        }
    }
}

void MonitorManager::updateSockets(MonitorOrScraper m,
                                   const std::string &eventType,
                                   const std::string &socketName,
                                   const std::string &socketFullPath) {
    std::string type{m == MonitorOrScraper::Monitor ? "Monitor." : "Scraper."};
    const auto typeLen = type.length();
    if (socketName.substr(0, typeLen) == type) {
        auto &map =
            m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
        std::string className{
            socketName.substr(typeLen, socketName.length() - typeLen)};
        auto it = map.find(className);
        if (eventType == "IN_CREATE") {
            KDBG("Socket was created, className: " + className);
            if (it != map.end()) {
                auto &storedObject = it->second;
                storedObject.p_socket =
                    std::make_unique<local::stream_protocol::endpoint>(
                        socketFullPath);
                if (storedObject.p_isBeingAdded) {
                    storedObject.p_confirmAdded = true;
                    storedObject.p_timer->cancel();
                }
            } else {
                StoredObject obj(className);
                obj.p_socket =
                    std::make_unique<local::stream_protocol::endpoint>(
                        socketFullPath);
                map.emplace(std::make_pair(className, std::move(obj)));
            }
        } else if (it != map.end()) {
            KDBG("Socket was destroyed, className: " + className);
            removeStoredSocket(map, it);
        }
    }
}

void terminateProcesses(
    std::unordered_map<std::string, StoredObject> &storedObjects) {
    for (auto &it : storedObjects) {
        auto &storedProcess = it.second.p_process;
        if (storedProcess) {
            auto &process = storedProcess->getProcess();
            if (process.running())
                process.terminate();
            // removeStoredProcess(storedObjects, it.second);
            /* should not be necessary since we are destructing */
        }
    }
}

MonitorManager::~MonitorManager() {
    terminateProcesses(_storedMonitors);
    terminateProcesses(_storedScrapers);
}

void checkProcessesMap(
    std::shared_ptr<spdlog::logger> &logger,
    std::unordered_map<std::string, StoredObject> &storedObjects) {
    for (auto it = storedObjects.begin(); it != storedObjects.end();) {
        auto &storedProcess = it->second.p_process;
        if (storedProcess) {
            auto &process = storedProcess->getProcess();
            if (!process.running()) {
                logger->warn("Monitor {} has exited with code {}", it->first,
                             process.exit_code());
                // process.join();
                removeStoredProcess(storedObjects, it);
            } else
                ++it;
        } else
            ++it;
    }
}

void MonitorManager::checkProcesses(const error_code &ec) {
    if (ec) {
        return;
    }
    checkProcessesMap(_logger, _storedMonitors);
    checkProcessesMap(_logger, _storedScrapers);
    _processCheckTimer.expires_after(std::chrono::milliseconds(500));
    _processCheckTimer.async_wait(
        std::bind(&MonitorManager::checkProcesses, this, ph::_1));
}

void MonitorManager::shutdown(const Cmd &cmd, const UserResponseCallback &&cb,
                              Connection::Ptr connection) {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
    _processCheckTimer.cancel();
    _fileWatcher.inotify.Close();
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

    auto &storedObjects =
        m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;

    auto it = storedObjects.find(className);
    if (it != storedObjects.end()) {
        if (it->second.p_isBeingAdded) {
            response.setError(genericError);
            response.setInfo(
                std::string{
                    (m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper")} +
                " still being processed.");
            cb(response, connection);
            return;
        }
        if (it->second.p_process) {
            response.setError(genericError);
            response.setInfo(
                std::string{
                    (m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper")} +
                " already started.");
            cb(response, connection);
            return;
        }
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
    auto delayTimer =
        std::make_shared<steady_timer>(_io, std::chrono::seconds(2));

    if (it != storedObjects.end()) {
        StoredObject &obj = it->second;
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null);
        obj.p_timer = delayTimer;
    } else {
        StoredObject obj{className};
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null);
        obj.p_isBeingAdded = true;
        obj.p_timer = delayTimer;
        storedObjects.emplace(std::make_pair(className, std::move(obj)));
    }

    delayTimer->async_wait([=](const error_code &ec) {
        auto &storedObjects =
            m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
        auto it = storedObjects.find(className);
        StoredObject &storedObject = it->second;
        storedObject.p_isBeingAdded = false;
        if ((!ec && storedObject.p_process->getProcess().running()) ||
            (ec == boost::system::errc::operation_canceled &&
             storedObject.p_confirmAdded)) {
            storedObject.p_confirmAdded = false;
            _logger->info("Correctly added {} {}",
                          m == MonitorOrScraper::Monitor ? "monitor "
                                                         : "scraper ",
                          className);
            cb(Response::okResponse(), connection);
        } else if (ec) {
            _logger->error(ec.message());
            Response response{Response::badResponse()};
            const std::string msg{"Internal timer failed with message: " +
                                  ec.message()};
            response.setInfo(msg);
            _logger->warn(msg);
            removeStoredProcess(storedObjects, it);
            cb(response, connection);
        } else {
            Response response;
            const std::string msg{
                "Process exited too soon. Exit code: " +
                std::to_string(
                    storedObject.p_process->getProcess().exit_code()) +
                "."};
            response.setError(genericError);
            response.setInfo(msg);
            _logger->info(msg);
            removeStoredProcess(storedObjects, it);
            cb(response, connection);
        }
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

    for (const auto &it :
         m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers) {
        const auto &storedObject = it.second;
        if (storedObject.p_process) {
            monitoredProcesses[storedObject.p_className] =
                storedObject.p_process->toJson();
        }
        if (storedObject.p_socket) {
            monitoredSockets[storedObject.p_className] =
                storedObject.p_socket->path();
        }
    }
    payload["monitored_processes"] = monitoredProcesses;
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
    ERRORS genericError = m == MonitorOrScraper::Monitor
                              ? ERRORS::MM_COULDNT_STOP_MONITOR
                              : ERRORS::MM_COULDNT_STOP_SCRAPER;
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

    auto &storedObjects =
        m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;

    auto it = storedObjects.find(className);
    if (it == storedObjects.end() || !it->second.p_socket) {
        response.setError(ERRORS::SOCKET_DOESNT_EXIST);
        response.setInfo(std::string{m == MonitorOrScraper::Monitor
                                         ? "Monitor "
                                         : "Scraper "} +
                         className + " not present in available sockets.");
        cb(response, connection);
        return;
    }

    auto &storedObject = it->second;

    if (storedObject.p_isBeingStopped) {
        response.setError(genericError);
        response.setInfo(std::string{m == MonitorOrScraper::Monitor
                                         ? "Monitor "
                                         : "Scraper "} +
                         className + " is already being stopped.");
        cb(response, connection);
        return;
    }

    storedObject.p_isBeingStopped = true;
    auto &ep = storedObject.p_socket;
    auto newConn = Connection::create(_io);
    newConn->socket.connect(*ep);
    Cmd newCmd;
    newCmd.setCmd(COMMANDS::STOP);
    newConn->asyncWriteCmd(newCmd, [=](const error_code &errc,
                                       Connection::Ptr conn) {
        auto &storedObjects =
            m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
        auto it = storedObjects.find(className);
        if (it != storedObjects.end()) {
            removeStoredSocket(storedObjects, it);
        }
        if (!errc) {
            KDBG("Sent STOP");
            conn->asyncReadResponse([=](const error_code &errc,
                                        const Response &response,
                                        Connection::Ptr conn) {
                if (errc)
                    KDBG(errc.message());
                cb(response, conn);
            });
        } else {
            KDBG(errc.message());
            Response response;
            response.setError(genericError);
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
