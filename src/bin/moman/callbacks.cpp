#include "moman.hpp"
#include <boost/process/detail/on_exit.hpp>
#include <functional>
#include <mongocxx/exception/query_exception.hpp>

namespace kekmonitors {

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

void MonitorManager::shutdown(const Cmd &cmd, const UserResponseCallback &&cb,
                              Connection::Ptr connection) {
    _logger->info("Shutting down...");
    KDBG("Received shutdown");
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
    auto &&onExitCb = std::bind(&MonitorManager::onProcessExit, this, ph::_1,
                                ph::_2, m, className);

    if (it != storedObjects.end()) {
        StoredObject &obj = it->second;
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null, _io,
            boost::process::on_exit(onExitCb));
        obj.p_onAddTimer = delayTimer;
    } else {
        StoredObject obj{className};
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null, _io,
            boost::process::on_exit(onExitCb));
        obj.p_isBeingAdded = true;
        obj.p_onAddTimer = delayTimer;
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
            KDBG("Sent STOP correctly");
            
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