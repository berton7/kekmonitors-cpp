#include "moman.hpp"
#include <boost/process/detail/on_exit.hpp>
#include <boost/system/detail/errc.hpp>
#include <functional>
#include <mongocxx/exception/query_exception.hpp>

namespace kekmonitors {

MonitorScraperCompletion::MonitorScraperCompletion(
    io_context &io, MonitorManager *moman, Cmd cmd,
    MonitorManagerCallback &&momanCb, DoubleResponseCallback &&completionCb,
    Connection::Ptr connection)
    : m_io(io), m_cmd(std::move(cmd)), m_completionCb(std::move(completionCb)),
      m_momanCb(std::move(momanCb)), m_moman(moman),
      m_connection(std::move(connection)) {
    KDBG("CTOR");
};

MonitorScraperCompletion::~MonitorScraperCompletion() { KDBG("dtor"); }

void MonitorScraperCompletion::run() {
    auto shared = shared_from_this();
    post(m_io, [=] {
        return shared->m_momanCb(
            shared->m_moman, MonitorOrScraper::Monitor, shared->m_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      ph::_1),
            m_connection);
    });
    post(m_io, [=] {
        return shared->m_momanCb(
            shared->m_moman, MonitorOrScraper::Scraper, shared->m_cmd,
            std::bind(&MonitorScraperCompletion::checkForCompletion, shared,
                      ph::_1),
            m_connection);
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
    if (!m_bothCompleted) {
        m_bothCompleted = true;
        m_firstResponse = response;
        return;
    }
    m_completionCb(m_firstResponse, response);
}

void MonitorManager::shutdown(const Cmd &cmd, const UserResponseCallback &&cb,
                              Connection::Ptr connection) {
    m_logger->info("Shutting down...");
    KDBG("Received shutdown");
    m_fileWatcher.inotify.Close();
    m_unixServer.shutdown();
    terminateProcesses(_storedMonitors);
    terminateProcesses(_storedScrapers);
    cb(Response::okResponse(), connection);
}

void MonitorManager::onPing(const Cmd &cmd, const UserResponseCallback &&cb,
                            Connection::Ptr connection) {
    m_logger->info("onPing callback!");
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

    if (cmd.payload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response, connection);
        return;
    }

    std::string className;
    const json &payload = cmd.payload();

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

    auto &registerDb = m == MonitorOrScraper::Monitor ? m_monitorRegisterDb
                                                      : m_scraperRegisterDb;

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
        m_logger->debug("Tried to add {} {} but it was not registered",
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
        std::make_shared<steady_timer>(m_io, std::chrono::seconds(2));
    auto &&onExitCb = std::bind(&MonitorManager::onProcessExit, this, ph::_1,
                                ph::_2, m, className);

    if (it != storedObjects.end()) {
        StoredObject &obj = it->second;
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null, m_io,
            boost::process::on_exit(onExitCb));
        obj.p_onAddTimer = delayTimer;
    } else {
        StoredObject obj{className};
        obj.p_process = std::make_unique<Process>(
            className, pythonExecutable + " " + path,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null, m_io,
            boost::process::on_exit(onExitCb));
        obj.p_isBeingAdded = true;
        obj.p_onAddTimer = delayTimer;
        storedObjects.emplace(std::make_pair(className, std::move(obj)));
    }

    delayTimer->async_wait([=](const error_code &ec) {
        /*
onAdd possible outcomes:
 1) NO OUTCOME: MonitorManager destructor => timer.cancel() --> return;
 2) FAIL: process exits sooner than timer => onProcessExit --> timer.cancel(),
    process removed, server/connection callback -> in async_wait: check for
iterator
 3) OK: process does not exit sooner, socket gets created => onSocketUpdate =>
    timer.cancel(), confirmAdded = true -> in async_wait: check for confirmAdded
 4) OK: process does not exit sooner, socket not created => async_wait
        */

        if (ec) {
            if (ec == boost::system::errc::operation_canceled) {
                auto &map = m == MonitorOrScraper::Monitor ? _storedMonitors
                                                           : _storedScrapers;
                auto it = map.find(className);
                if (it == map.end()) // className not found => 2)
                {
                    Response response = Response::badResponse();
                    response.setError(genericError);
                    response.setInfo("Process exited sooner than expected.");
                    cb(response, connection);
                    return;
                } else // className found
                {
                    StoredObject &storedObject = it->second;
                    storedObject.p_isBeingAdded = false;
                    if (storedObject.p_confirmAdded) // => 3)
                    {
                        storedObject.p_confirmAdded = false;
                        cb(Response::okResponse(), connection);
                        return;
                    }
                }

                // everything else is 1)
            }
        } else {
            auto &map = m == MonitorOrScraper::Monitor ? _storedMonitors
                                                       : _storedScrapers;
            auto it = map.find(className);
            if (it == map.end()) {
                Response response = Response::badResponse();
                response.setError(genericError);
                response.setInfo(
                    "Process exited sooner than expected. (WARNING: THIS CASE "
                    "(1) SHOULD NOT HAVE HAPPENED -- NEED TO DEBUG!!!)");
                cb(response, connection);
                return;
            } else {
                auto &storedObject = it->second;
                if (storedObject.p_process->process().running()) {
                    storedObject.p_isBeingAdded = false;
                    cb(Response::okResponse(), connection);
                    return;
                } else {
                    Response response = Response::badResponse();
                    response.setError(genericError);
                    response.setInfo("Process exited sooner than expected. "
                                     "(WARNING: THIS CASE (2) SHOULD NOT HAVE "
                                     "HAPPENED -- NEED TO DEBUG!!!)");
                    cb(response, connection);
                    return;
                }
            }
        }
    });
}

void MonitorManager::onAddMonitorScraper(const Cmd &cmd,
                                         const UserResponseCallback &&cb,
                                         Connection::Ptr connection) {
    MonitorScraperCompletion::create(
        m_io, this, cmd, &MonitorManager::onAdd,
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
        m_io, this, cmd, &MonitorManager::onGetStatus,
        [=](const Response &firstResponse, const Response &secondResponse) {
            Response response{
                utils::makeCommonResponse(firstResponse, secondResponse)};
            json payload;
            json tmpPayload = firstResponse.payload();
            payload["monitors"] =
                tmpPayload.is_null() ? json::object() : tmpPayload;
            tmpPayload = secondResponse.payload();
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

    if (cmd.payload() == nullptr) {
        response.setError(ERRORS::MISSING_PAYLOAD);
        cb(response, connection);
        return;
    }

    std::string className;
    const json &payload = cmd.payload();

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
    auto newConn = Connection::create(m_io);
    newConn->p_socket.connect(*ep);
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
        m_io, this, cmd, &MonitorManager::onStop,
        [=](const Response &firstResponse, const Response &secondResponse) {
            cb(utils::makeCommonResponse(
                   firstResponse, secondResponse,
                   ERRORS::MM_COULDNT_STOP_MONITOR_SCRAPER),
               connection);
        },
        connection);
}
} // namespace kekmonitors