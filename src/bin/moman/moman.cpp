#include "moman.hpp"
#include "kekmonitors/core.hpp"
#include "spdlog/common.h"
#include "spdlog/logger.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/process/detail/on_exit.hpp>
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

MonitorManager::MonitorManager(io_context &io, std::shared_ptr<Config> config)
    : m_io(io), m_fileWatcher(io), m_config(std::move(config)) {
    if (!m_config)
        m_config = std::make_shared<Config>();
    m_logger = utils::getLogger("MonitorManager");
    m_kekDbConnection = std::make_unique<mongocxx::client>(mongocxx::uri{
        m_config->p_parser.get<std::string>("GlobalConfig.db_path")});
    m_kekDb = (*m_kekDbConnection)[m_config->p_parser.get<std::string>(
        "GlobalConfig.db_name")];
    m_monitorRegisterDb = m_kekDb["register.monitors"];
    m_scraperRegisterDb = m_kekDb["register.scrapers"];
#ifdef KEKMONITORS_DEBUG
    KDBG("Removing leftover monitor manager socket. Beware this is a "
         "debug-only feature.");
    ::unlink(std::string{utils::getLocalKekDir() + "/sockets/MonitorManager"}
                 .c_str());
#endif
    m_unixServer = std::make_unique<UnixServer>(
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
        m_config);
    m_fileWatcher.watches.emplace_back(
        m_config->p_parser.get<std::string>("GlobalConfig.socket_path"),
        IN_ALL_EVENTS);
    m_fileWatcher.inotify.Add(m_fileWatcher.watches[0]);
    m_fileWatcher.inotify.AsyncStartWaitForEvents(
        std::bind(&MonitorManager::onInotifyUpdate, this));
}

void MonitorManager::onInotifyUpdate() {
    size_t count = m_fileWatcher.inotify.GetEventCount();
    for (; count > 0; --count) {
        InotifyEvent event;
        bool got_event = m_fileWatcher.inotify.GetEvent(&event);
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

void MonitorManager::onProcessExit(int exit, const std::error_code &ec,
                                   MonitorOrScraper m,
                                   const std::string &className) {
    std::string monitorOrScraper{m == MonitorOrScraper::Monitor ? "Monitor"
                                                                : "Scraper"};
    if (ec) {
        m_logger->error("Error for {} {}: {}", monitorOrScraper, className,
                       ec.message());
    } else {
        m_logger->log(exit ? spdlog::level::warn : spdlog::level::info,
                     "Monitor {} has exited with code {}", className, exit);
        auto &map =
            m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
        auto it = map.find(className);
        if (it != map.end()) {
            removeStoredProcess(map, it);
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
                    storedObject.p_onAddTimer->cancel();
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
    for (auto it = storedObjects.begin(); it != storedObjects.end();) {
        auto &storedProcess = it->second.p_process;
        if (storedProcess) {
            auto &process = storedProcess->process();
            if (process.running())
                process.terminate();
            removeStoredProcess(storedObjects, it);
        } else
            ++it;
    }
}

MonitorManager::~MonitorManager() {
}

} // namespace kekmonitors

int main() {
    io_context io;
    kekmonitors::init();
    kekmonitors::MonitorManager moman(io);
    io.run();
    return 0;
}
