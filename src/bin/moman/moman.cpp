#include "moman.hpp"
#include "kekmonitors/inotify-cxx.h"
#include "kekmonitors/msg.hpp"
#include "spdlog/common.h"
#include "spdlog/fmt/bundled/core.h"
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
    m_db = m_dbClient[config.p_parser.get<std::string>("GlobalConfig.db_name")];
    m_monitorRegisterDb = m_db["register.monitors"];
    m_scraperRegisterDb = m_db["register.scrapers"];

    for (const auto &file :
         fs::directory_iterator{utils::getLocalKekDir() + "/sockets/"}) {
        const auto &filepath = file.path();
        const auto filepathStr = filepath.string();
        const std::string filename = filepath.filename();
        if (is_socket(filepathStr) && filename != "MonitorManager") {
            checkSocketAndUpdateList(filepathStr, filename, IN_CREATE);
        }
    }

    m_fileWatcher.inotify.Add(m_fileWatcher.watches.emplace_back(
        config.p_parser.get<std::string>("GlobalConfig.socket_path"),
        IN_CREATE | IN_DELETE));
    const fs::path configDir = utils::getLocalKekDir() + "/config";
    m_fileWatcher.inotify.Add(m_fileWatcher.watches.emplace_back(
        configDir.string(), IN_CREATE | IN_DELETE));
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
                    checkSocketAndUpdateList(socketFullPath, filename,
                                             eventType);
                }
            } else {
                const auto allowedConfigSubDir = {"common", "monitors",
                                                  "scrapers"};
                const auto allowedFilenames = {"whitelists.json",
                                               "blacklists.json",
                                               "webhooks.json", "configs.json"};
                // since IN_ISDIR might be set, remove it.
                // we can still retrieve it later.
                auto m = eventType & ~IN_ISDIR;
                switch (m) {
                case IN_CREATE:
                    if (eventType & IN_ISDIR) {
                        if (std::find(allowedConfigSubDir.begin(),
                                      allowedConfigSubDir.end(),
                                      filename) != allowedConfigSubDir.end()) {
                            m_fileWatcher.inotify.Add(
                                m_fileWatcher.watches.emplace_back(
                                    fullEventPath, IN_CREATE | IN_DELETE));
                            m_logger->info(
                                "New config folder created and monitored: {}",
                                fullEventPath);
                        }
                        break;
                    } else {
                        if (std::find(allowedFilenames.begin(),
                                      allowedFilenames.end(),
                                      filename) != allowedFilenames.end()) {
                            bool alreadyMonitored{false};
                            for (const auto &watch : m_fileWatcher.watches) {
                                if (watch.GetPath() == fullEventPath) {
                                    alreadyMonitored = true;
                                    break;
                                }
                            }
                            if (!alreadyMonitored) {
                                m_fileWatcher.inotify.Add(
                                    m_fileWatcher.watches.emplace_back(
                                        fullEventPath, IN_MODIFY));
                                m_logger->info(
                                    "New config file created and monitored: {}",
                                    fullEventPath);
                                parseAndSendConfigs(fullEventPath, filename);
                                break;
                            }

                            // if alreadyMonitored it's probably been edited
                            // go ahead and update
                        } else
                            break;
                    }
                case IN_MODIFY:
                case IN_CLOSE_NOWRITE:
                case IN_CLOSE_WRITE:
                    m_logger->info("File {} has changed", fullEventPath);
                    parseAndSendConfigs(fullEventPath, filename);
                    break;
                case IN_DELETE:
                    auto &allowedNames = eventType & IN_ISDIR
                                             ? allowedConfigSubDir
                                             : allowedFilenames;
                    if (std::find(allowedNames.begin(), allowedNames.end(),
                                  filename) != allowedNames.end()) {
                        for (auto watch = m_fileWatcher.watches.begin();
                             watch != m_fileWatcher.watches.end(); ++watch) {
                            if (watch->GetPath() == fullEventPath) {
                                m_fileWatcher.inotify.Remove(*watch);
                                m_fileWatcher.watches.erase(watch);
                                m_logger->warn("Config {} was removed: {}",
                                               eventType & IN_ISDIR ? "folder"
                                                                    : "file",
                                               fullEventPath);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

void MonitorManager::parseAndSendConfigs(const std::string &fullPath,
                                         const std::string &filename) {
    std::ifstream configStream(fullPath);
    if (!configStream.is_open()) {
        m_logger->error("Error while opening file {}", fullPath);
    }
    json configJson;
    try {
        configStream >> configJson;
        configStream.close();
    } catch (json::parse_error &e) {
        m_logger->warn("Config file {} is not in valid json", fullPath);
        return;
    }
    if (!configJson.is_object()) {
        m_logger->warn("Config file {} is not a dict", fullPath);
        return;
    }
    Cmd cmd;
    std::string configSubDir;
    bool flag{false};
    size_t lastSlash{0};
    for (size_t i = fullPath.length() - 1; i >= 0; --i) {
        if (fullPath[i] == fs::path::preferred_separator) {
            if (!flag) {
                lastSlash = i;
                flag = true;
            } else {
                configSubDir = fullPath.substr(i + 1, lastSlash - i - 1);
                break;
            }
        }
    }
    if (configSubDir.empty()) {
        m_logger->error("Could not understand configSubDir");
        return;
    }

    if (filename == "webhooks.json")
        cmd.setCmd(configSubDir == "common" ? COMMANDS::SET_COMMON_WEBHOOKS
                                            : COMMANDS::SET_SPECIFIC_WEBHOOKS);
    else if (filename == "whitelists.json")
        cmd.setCmd(configSubDir == "common" ? COMMANDS::SET_COMMON_WHITELIST
                                            : COMMANDS::SET_SPECIFIC_WHITELIST);
    else if (filename == "blacklists.json")
        cmd.setCmd(configSubDir == "common" ? COMMANDS::SET_COMMON_BLACKLIST
                                            : COMMANDS::SET_SPECIFIC_BLACKLIST);
    else if (filename == "configs.json")
        cmd.setCmd(configSubDir == "common" ? COMMANDS::SET_COMMON_CONFIG
                                            : COMMANDS::SET_SPECIFIC_CONFIG);

    for (json::const_iterator it = configJson.cbegin(); it != configJson.cend();
         ++it) {
        const std::string &className = it.key();
        cmd.setPayload(configJson.at(className));
        auto s = cmd.toString();
        if (configSubDir == "monitors" || configSubDir == "common") {
            sendCmdIfProcess(MonitorOrScraper::Monitor, cmd, className);
        };
        if (configSubDir == "scrapers" || configSubDir == "common") {
            sendCmdIfProcess(MonitorOrScraper::Scraper, cmd, className);
        }
    }
}

void MonitorManager::sendCmdIfProcess(MonitorOrScraper m, const Cmd &cmd,
                                      const std::string &className) {
    auto &storedObjects =
        m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
    const auto it = storedObjects.find(className);
    if (it != storedObjects.end()) {
        if (it->second.p_process && it->second.p_endpoint) {
            auto connection = Connection::create(m_io);
            const auto mstring =
                m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper";
            connection->p_endpoint.async_connect(
                *(it->second.p_endpoint), [=](const error_code &errc) {
                    if (errc) {
                        if (errc != boost::system::errc::operation_canceled) {
                            m_logger->error(
                                "Error trying to connect to {} {}: {}", mstring,
                                className, errc.message());
                        }
                        return;
                    }
                    connection->asyncWriteCmd(cmd, [=](const error_code &errc,
                                                       Connection::Ptr
                                                           connection) {
                        if (errc) {
                            if (errc !=
                                boost::system::errc::operation_canceled) {
                                m_logger->error(
                                    "Error trying to write cmd to {} {}: {}",
                                    mstring, className, errc.message());
                            }
                            return;
                        }
                        connection->asyncReadResponse([=](const error_code
                                                              &errc,
                                                          const Response &r,
                                                          Connection::Ptr) {
                            if (errc) {
                                if (errc !=
                                    boost::system::errc::operation_canceled) {
                                    m_logger->error("Error trying to read "
                                                    "response from {} {}: {}",
                                                    mstring, className,
                                                    errc.message());
                                }
                                return;
                            }
                            if (r.error()) {
                                m_logger->warn(r.toString());
                            }
                        });
                    });
                });
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
                      "{} {} has exited with code {}", monitorOrScraper,
                      className, exit);
        auto &map =
            m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
        auto it = map.find(className);
        if (it != map.end()) {
            removeStoredProcess(map, it);
        }
    }
}

void MonitorManager::verifySocketIsCommunicating(
    MonitorOrScraper m, const std::string &socketFullPath,
    const std::string &className, std::function<void()> &&on_success) {

    auto newConn = Connection::create(m_io);
    const auto &&on_connect = [=]() {
        Cmd newCmd;
        newCmd.setCmd(COMMANDS::PING);
        newConn->quickWriteCmd(
            newCmd,
            [=](const Response &resp) {
                if (resp.error()) {
                    m_logger->debug("{} {} responded with error",
                                    m == MonitorOrScraper::Monitor ? "Monitor"
                                                                   : "Scraper",
                                    className);
                    return;
                }
                m_logger->info("{} {} found",
                               m == MonitorOrScraper::Monitor ? "Monitor"
                                                              : "Scraper",
                               className);
                on_success();
            },
            [=](const error_code &errc) {
                m_logger->warn("Failed to communicate with {} {}, error: {}",
                               m == MonitorOrScraper::Monitor ? "Monitor"
                                                              : "Scraper",
                               className, errc.message());
            });
    };

    error_code errc;
    auto ep = local::stream_protocol::endpoint{socketFullPath};
    newConn->p_endpoint.connect(ep, errc);
    if (errc) {
        steady_timer timer{m_io, std::chrono::milliseconds(500)};
        timer.async_wait([=](const error_code &timer_errc) {
            if (!timer_errc) {
                error_code socket_errc;
                newConn->p_endpoint.connect(ep, socket_errc);
                if (!socket_errc)
                    on_connect();
                else
                    m_logger->debug("{} {} found but not available",
                                    m == MonitorOrScraper::Monitor ? "Monitor"
                                                                   : "Scraper",
                                    className);
            }
        });
        return;
    }
};

void MonitorManager::checkSocketAndUpdateList(const std::string &socketFullPath,
                                              std::string socketName,
                                              uint32_t mask) {
    if (socketName.empty()) {
        socketName = socketFullPath.substr(
            socketFullPath.rfind(fs::path::preferred_separator) + 1);
    }
    auto dotIndex = socketName.rfind(".");
    if (dotIndex == std::string::npos) {
        m_logger->error("Invalid socket path: " + socketFullPath);
        return;
    }
    std::string socketPrefix = socketName.substr(0, dotIndex);
    MonitorOrScraper m;
    if (socketPrefix == "Monitor") {
        m = MonitorOrScraper::Monitor;
    } else if (socketPrefix == "Scraper") {
        m = MonitorOrScraper::Scraper;
    } else {
        return;
    }
    auto &map =
        m == MonitorOrScraper::Monitor ? _storedMonitors : _storedScrapers;
    std::string className{socketName.substr(dotIndex + 1)};
    auto it = map.find(className);
    switch (mask) {
    case IN_CREATE:
        if (it != map.end()) {
            auto &storedObject = it->second;
            storedObject.p_endpoint =
                std::make_unique<local::stream_protocol::endpoint>(
                    socketFullPath);
            if (storedObject.p_isBeingAdded) {
                verifySocketIsCommunicating(
                    m, socketFullPath, className, [=]() {
                        auto &storedObject = it->second;
                storedObject.p_confirmAdded = true;
                storedObject.p_onAddTimer->cancel();
                        m_logger->info(fmt::format(
                            "{} {} added",
                            m == MonitorOrScraper::Monitor ? "Monitor"
                                                           : "Scraper",
                            className));
                    });
            } else {
                m_logger->warn(fmt::format(
                    "{} {}: found socket but not synchronized with "
                    "MonitorManager",
                    m == MonitorOrScraper::Monitor ? "Monitor" : "Scraper",
                    className));
            }
        } else {
            verifySocketIsCommunicating(m, socketFullPath, className, [=]() {
            StoredObject obj(className);
                obj.p_endpoint =
                    std::make_unique<local::stream_protocol::endpoint>(
                socketFullPath);
                auto &map = m == MonitorOrScraper::Monitor ? _storedMonitors
                                                           : _storedScrapers;
                auto pair =
            map.emplace(std::make_pair(className, std::move(obj)));
            });
        };
        break;
    case IN_DELETE:
        if (it != map.end()) {
            KDBG(fmt::format("Socket {} was removed", className));
            removeStoredSocket(map, it);
            break;
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
