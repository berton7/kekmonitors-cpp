#pragma once
#include "server.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/process.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <string>
#include <unordered_map>

namespace kekmonitors {

typedef struct {
    Inotify inotify;
    std::vector<InotifyWatch> watches;
    std::thread watchThread;
} FileWatcher;

class MonitorManager {
  private:
    io_context &_io;
    steady_timer _processCheckTimer;
    std::unique_ptr<UnixServer> _unixServer{nullptr};
    std::shared_ptr<Config> _config{nullptr};
    std::shared_ptr<spdlog::logger> _logger{nullptr};
    std::unique_ptr<mongocxx::client> _kekDbConnection{nullptr};
    mongocxx::database _kekDb{};
    mongocxx::collection _monitorRegisterDb{};
    mongocxx::collection _scraperRegisterDb{};
    std::atomic<bool> _fileWatcherStop{false};
    FileWatcher _fileWatcher;
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _monitorProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _tmpMonitorProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _scraperProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _tmpScraperProcesses{};
    std::mutex _socketLock{};
    std::unordered_map<std::string, local::stream_protocol::endpoint>
        _monitorSockets{};
    std::unordered_map<std::string, local::stream_protocol::endpoint>
        _scraperSockets{};

    void checkProcesses(const error_code &);

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
    io_service &_io;
    const Cmd _cmd;
    const DoubleResponseCallback _completionCb;
    const MonitorManagerCallback _momanCb;
    MonitorManager *_moman;
    const std::shared_ptr<Connection> _connection;
    Response _firstResponse;

  public:
    MonitorScraperCompletion() = delete;
    MonitorScraperCompletion(io_service &io, MonitorManager *moman, Cmd cmd,
                             MonitorManagerCallback &&momanCb,
                             DoubleResponseCallback &&completionCb,
                             std::shared_ptr<Connection> connection);

    ~MonitorScraperCompletion();

    void run();

    static void create(io_service &io, MonitorManager *moman, const Cmd &cmd,
                       MonitorManagerCallback &&momanCb,
                       DoubleResponseCallback &&completionCb,
                       std::shared_ptr<Connection> connection);

  private:
    void checkForCompletion(const Response &response);
};
} // namespace kekmonitors
