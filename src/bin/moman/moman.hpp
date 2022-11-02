#pragma once
#include "server.hpp"
#include <boost/asio/detail/cstdint.hpp>
#include <boost/asio/steady_timer.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/inotify-cxx.h>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/process.hpp>
#include <list>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <string>
#include <unordered_map>

namespace kekmonitors {

class FileWatcher {
  public:
    FileWatcher(io_context &io) : inotify(io) {}
    Inotify inotify;
    std::list<InotifyWatch> watches;
};

class StoredObject {
  public:
    std::unique_ptr<Process> p_process{nullptr};
    std::unique_ptr<local::stream_protocol::endpoint> p_endpoint{nullptr};
    std::shared_ptr<steady_timer> p_onAddTimer{nullptr};
    std::shared_ptr<steady_timer> p_onStopTimer{nullptr};
    UserResponseCallback p_stopCallback;
    std::string p_className{};
    bool p_isBeingAdded{false};
    bool p_isBeingStopped{false};
    bool p_confirmAdded{false};

    StoredObject(std::string className) : p_className(std::move(className)){};
    StoredObject(StoredObject &&other)
        : p_process{std::move(other.p_process)}, p_endpoint{std::move(
                                                     other.p_endpoint)},
          p_onAddTimer(std::move(other.p_onAddTimer)),
          p_onStopTimer(std::move(other.p_onStopTimer)),
          p_stopCallback(std::move(other.p_stopCallback)),
          p_className{std::move(other.p_className)},
          p_isBeingAdded(std::move(other.p_isBeingAdded)),
          p_isBeingStopped(std::move(other.p_isBeingStopped)),
          p_confirmAdded(std::move(other.p_confirmAdded)) {
        other.p_isBeingAdded = false;
        other.p_isBeingStopped = false;
        other.p_confirmAdded = false;
    }
    ~StoredObject() = default;
};

class MonitorManager {
  private:
    io_context &m_io;
    UnixServer m_unixServer;
    std::shared_ptr<spdlog::logger> m_logger{nullptr};
    mongocxx::client m_dbClient;
    mongocxx::database m_db;
    mongocxx::collection m_monitorRegisterDb;
    mongocxx::collection m_scraperRegisterDb;
    std::atomic<bool> m_fileWatcherStop{false};
    FileWatcher m_fileWatcher;
    std::unordered_map<std::string, StoredObject> _storedMonitors;
    std::unordered_map<std::string, StoredObject> _storedScrapers;

    void onInotifyUpdate();
    void onProcessExit(int exit, const std::error_code &, MonitorOrScraper,
                       const std::string &className);

    void checkSocketAndUpdateList(const std::string &socketFullPath,
                       std::string socketName = "", uint32_t mask = 0);

    void parseAndSendConfigs(const std::string &fullPath,
                             const std::string &filename);

    void sendCmdIfProcess(const MonitorOrScraper, const Cmd &cmd,
                 const std::string &className);

    void verifySocketIsCommunicating(MonitorOrScraper m,
                                     const std::string &socketFullPath,
                                     const std::string &className,
                                     std::function<void()> &&on_success);

  public:
    MonitorManager() = delete;
    explicit MonitorManager(boost::asio::io_context &io);
    ~MonitorManager();
    void shutdown(const Cmd &cmd, const UserResponseCallback &&cb,
                  Connection::Ptr connection);
    void onPing(const Cmd &cmd, const UserResponseCallback &&cb,
                Connection::Ptr connection);
    void onAdd(MonitorOrScraper m, const Cmd &cmd,
               const UserResponseCallback &&cb, Connection::Ptr connection);
    void onAddMonitorScraper(const Cmd &cmd, const UserResponseCallback &&cb,
                             Connection::Ptr connection);
    void onStop(MonitorOrScraper m, const Cmd &cmd,
                const UserResponseCallback &&cb, Connection::Ptr connection);
    void onStopMonitorScraper(const Cmd &cmd, const UserResponseCallback &&cb,
                              Connection::Ptr connection);
    void onGetStatus(MonitorOrScraper m, const Cmd &cmd,
                     const UserResponseCallback &&cb,
                     Connection::Ptr connection);
    void onGetMonitorScraperStatus(const Cmd &cmd,
                                   const UserResponseCallback &&cb,
                                   Connection::Ptr connection);
};

typedef std::function<void(MonitorManager *, MonitorOrScraper m, const Cmd &cmd,
                           const UserResponseCallback &&cb, Connection::Ptr)>
    MonitorManagerCallback;
typedef std::function<void(const kekmonitors::Response &,
                           const kekmonitors::Response &)>
    DoubleResponseCallback;

class MonitorScraperCompletion
    : public std::enable_shared_from_this<MonitorScraperCompletion> {

  private:
    bool m_bothCompleted{false};
    io_context &m_io;
    const Cmd m_cmd;
    const DoubleResponseCallback m_completionCb;
    const MonitorManagerCallback m_momanCb;
    MonitorManager *m_moman;
    const std::shared_ptr<Connection> m_connection;
    Response m_firstResponse;

  public:
    MonitorScraperCompletion() = delete;
    MonitorScraperCompletion(io_context &io, MonitorManager *moman, Cmd cmd,
                             MonitorManagerCallback &&momanCb,
                             DoubleResponseCallback &&completionCb,
                             std::shared_ptr<Connection> connection);

    ~MonitorScraperCompletion();

    void run();

    static void create(io_context &io, MonitorManager *moman, const Cmd &cmd,
                       MonitorManagerCallback &&momanCb,
                       DoubleResponseCallback &&completionCb,
                       std::shared_ptr<Connection> connection);

  private:
    void checkForCompletion(const Response &response);
};

template <typename Map, typename Iterator>
void removeStoredSocket(Map &map, Iterator &it) {
    auto &stored = it->second;
    stored.p_endpoint = nullptr;
    if (!stored.p_process)
        it = map.erase(it);
}

template <typename Map, typename Iterator>
void removeStoredProcess(Map &map, Iterator &it) {
    auto &stored = it->second;
    stored.p_process = nullptr;
    if (!stored.p_endpoint)
        it = map.erase(it);
}

void terminateProcesses(
    std::unordered_map<std::string, StoredObject> &storedObjects);

} // namespace kekmonitors
