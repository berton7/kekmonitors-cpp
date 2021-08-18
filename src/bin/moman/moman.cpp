#include "moman.hpp"
#include "kekmonitors/inotify-cxx.h"
#include "kekmonitors/msg.hpp"
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
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <kekmonitors/core.hpp>
#include <kekmonitors/utils.hpp>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/query_exception.hpp>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
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

MonitorManager::MonitorManager(io_context &io)
    : m_io(io), m_fileWatcher(io),
      m_unixServer(
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
                                &MonitorManager::onStopMonitorScraper)}) {
    m_logger = utils::getLogger("MonitorManager");
    const auto &config = getConfig();
    m_dbClient = mongocxx::client{mongocxx::uri{
        config.p_parser.get<std::string>("GlobalConfig.db_path")}};
    m_db = m_dbClient[config.p_parser.get<std::string>(
        "GlobalConfig.db_name")];
    m_monitorRegisterDb = m_db["register.monitors"];
    m_scraperRegisterDb = m_db["register.scrapers"];
    m_fileWatcher.inotify.Add(m_fileWatcher.watches.emplace_back(
        config.p_parser.get<std::string>("GlobalConfig.socket_path"),
        IN_CREATE | IN_DELETE));
    const fs::path configDir = utils::getLocalKekDir() + "/config";
    for (const auto &configSubDir : {"common", "monitors", "scrapers"}) {
        const fs::path configSubPath = configDir / configSubDir;
        if (fs::is_directory(configSubPath)) {
            m_fileWatcher.inotify.Add(m_fileWatcher.watches.emplace_back(
                configSubPath.string(), IN_CREATE | IN_DELETE));
            for (const auto &configFile : {"whitelists.json", "blacklists.json",
                                           "webhooks.json", "configs.json"}) {
                const fs::path configFilePath = configSubPath / configFile;
                if (fs::is_regular_file(configFilePath)) {
                    m_fileWatcher.inotify.Add(
                        m_fileWatcher.watches.emplace_back(
                            configFilePath.string(), IN_MODIFY));
                }
            }
        }
    }
    m_fileWatcher.inotify.AsyncStartWaitForEvents(
        std::bind(&MonitorManager::onInotifyUpdate, this));
    m_unixServer.startAccepting();
}

void MonitorManager::onInotifyUpdate() {
    for (size_t count = m_fileWatcher.inotify.GetEventCount(); count > 0;
         --count) {
        InotifyEvent event;
        bool got_event = m_fileWatcher.inotify.GetEvent(&event);
        const auto &filename = event.GetName();
        uint32_t eventType = event.GetMask();
        const auto eventPath = event.GetWatch()->GetPath();
        const auto fullEventPath =
            filename.empty()
                ? eventPath
                : eventPath + fs::path::preferred_separator + filename;
        if (got_event) {
            if (eventPath == getConfig().p_parser.get<std::string>(
                                 "GlobalConfig.socket_path")) {
                if (eventType & IN_CREATE && is_socket(fullEventPath) ||
                    eventType & IN_DELETE) {
                    std::string socketFullPath{
                        eventPath + fs::path::preferred_separator + filename};
                    updateSockets(MonitorOrScraper::Monitor, eventType,
                                  filename, socketFullPath);
                    updateSockets(MonitorOrScraper::Scraper, eventType,
                                  filename, socketFullPath);
                }
            } else {
                const auto allowedConfigSubDir = {"common", "monitors",
                                                  "scrapers"};
                const auto allowedFilenames = {"whitelists.json",
                                               "blacklists.json",
                                               "webhooks.json", "configs.json"};
                switch (eventType) {
                case IN_CREATE:
                    if (std::find(allowedFilenames.begin(),
                                  allowedFilenames.end(),
                                  filename) != allowedFilenames.end()) {
                        m_fileWatcher.watches.emplace_back(
                            eventPath, IN_CREATE | IN_DELETE | IN_DELETE_SELF);
                        m_logger->info(
                            "New config file created and monitored: {}",
                            fullEventPath);
                    }
                    break;
                case IN_MODIFY:
                    m_logger->info("File {} has changed", fullEventPath);
                    break;
                case IN_DELETE:
                    if (std::find(allowedFilenames.begin(),
                                  allowedFilenames.end(),
                                  filename) != allowedFilenames.end()) {
                        for (auto watch = m_fileWatcher.watches.begin();
                             watch != m_fileWatcher.watches.end();) {
                            if (watch->GetPath() == fullEventPath) {
                                m_fileWatcher.inotify.Remove(*watch);
                                m_fileWatcher.watches.erase(watch);
                                m_logger->warn("Config file was removed: {}",
                                               fullEventPath);
                                break;
                            } else
                                ++watch;
                        }
                    }
                    break;
                }
            }
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

void MonitorManager::updateSockets(MonitorOrScraper m, const uint32_t eventType,
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
        switch (eventType) {
        case IN_CREATE:
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
            break;
        case IN_DELETE:
            if (it != map.end()) {
                KDBG("Socket was destroyed, className: " + className);
                removeStoredSocket(map, it);
                break;
            }
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

MonitorManager::~MonitorManager() {}

} // namespace kekmonitors

int main() {
    io_context io;
    kekmonitors::init();
    kekmonitors::MonitorManager moman(io);
    io.run();
    return 0;
}
