#pragma once
#include "server.hpp"
#include <boost/asio/steady_timer.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/inotify-cxx.h>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/process.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <string>
#include <unordered_map>

namespace kekmonitors {

class FileWatcher {
  public:
    FileWatcher(io_context &io) : inotify(io) {}
    Inotify inotify;
    std::vector<InotifyWatch> watches;
};

class StoredObject {
  public:
    std::unique_ptr<Process> p_process{nullptr};
    std::unique_ptr<local::stream_protocol::endpoint> p_socket{nullptr};
    std::shared_ptr<steady_timer> p_onAddTimer{nullptr};
    std::shared_ptr<steady_timer> p_onStopTimer{nullptr};
    UserResponseCallback p_stopCallback;
    std::string p_className{};
    bool p_isBeingAdded{false};
    bool p_isBeingStopped{false};
    bool p_confirmAdded{false};

    StoredObject(std::string className) : p_className(std::move(className)){};
    StoredObject(StoredObject &&other)
        : p_process{std::move(other.p_process)}, p_socket{std::move(
                                                     other.p_socket)},
          p_onAddTimer(std::move(other.p_onAddTimer)),
          p_onStopTimer(std::move(other.p_onStopTimer)),
          p_stopCallback(std::move(other.p_stopCallback)),
          p_className{std::move(other.p_className)},
          p_isBeingAdded(std::move(other.p_isBeingAdded)),
          p_isBeingStopped(std::move(other.p_isBeingStopped)),
          p_confirmAdded(std::move(other.p_confirmAdded)) {
        other.p_className = "";
        other.p_isBeingAdded = false;
        other.p_isBeingStopped = false;
        other.p_confirmAdded = false;
    }
    ~StoredObject() = default;
};

class MonitorManager {
  private:
    io_context &_io;
    std::unique_ptr<UnixServer> _unixServer{nullptr};
    std::shared_ptr<Config> _config{nullptr};
    std::shared_ptr<spdlog::logger> _logger{nullptr};
    std::unique_ptr<mongocxx::client> _kekDbConnection{nullptr};
    mongocxx::database _kekDb{};
    mongocxx::collection _monitorRegisterDb{};
    mongocxx::collection _scraperRegisterDb{};
    std::atomic<bool> _fileWatcherStop{false};
    FileWatcher _fileWatcher;
    std::unordered_map<std::string, StoredObject> _storedMonitors;
    std::unordered_map<std::string, StoredObject> _storedScrapers;

    void onInotifyUpdate();
    void onProcessExit(int exit, const std::error_code &, MonitorOrScraper,
                       const std::string &className);

    void updateSockets(MonitorOrScraper, const std::string &eventType,
                       const std::string &socketName,
                       const std::string &socketFullPath);

  public:
    MonitorManager() = delete;
    explicit MonitorManager(boost::asio::io_context &io,
                            std::shared_ptr<Config> config = nullptr);
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
    bool _bothCompleted{false};
    io_context &_io;
    const Cmd _cmd;
    const DoubleResponseCallback _completionCb;
    const MonitorManagerCallback _momanCb;
    MonitorManager *_moman;
    const std::shared_ptr<Connection> _connection;
    Response _firstResponse;

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

void terminateProcesses(
    std::unordered_map<std::string, StoredObject> &storedObjects);

} // namespace kekmonitors
