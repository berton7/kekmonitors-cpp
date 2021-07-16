#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/process.hpp>
#include <kekmonitors/server.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

namespace kekmonitors {

typedef struct {
    Inotify inotify;
    std::vector<InotifyWatch> watches;
    std::thread watchThread;
    std::deque<InotifyEvent> eventQueue;
} FileWatcher;

class MonitorManager {
  private:
    io_context &_io;
    steady_timer _processCheckTimer;
    std::unique_ptr<UnixServer> _unixServer{nullptr};
    std::shared_ptr<Config> _config{nullptr};
    std::unique_ptr<spdlog::logger> _logger{nullptr};
    std::unique_ptr<mongocxx::client> _kekDbConnection{nullptr};
    mongocxx::database _kekDb{};
    mongocxx::collection _monitorRegisterDb{};
    mongocxx::collection _scraperRegisterDb{};
    std::mutex _fileWatcherLock;
    std::atomic<bool> _fileWatcherStop{false};
    std::atomic<bool> _fileWatcherAddEvent{false};
    FileWatcher _fileWatcher;
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _monitorProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _tmpMonitorProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _scraperProcesses{};
    std::unordered_map<std::string, std::shared_ptr<Process>>
        _tmpScraperProcesses{};

    void checkProcesses(const error_code &);

  public:
    MonitorManager() = delete;
    explicit MonitorManager(boost::asio::io_context &io,
                            std::shared_ptr<Config> config = nullptr);
    ~MonitorManager();
    void shutdown(const Cmd &cmd, const userResponseCallback &&cb);
    void onPing(const Cmd &cmd, const userResponseCallback &&cb);
    void onAdd(MonitorOrScraper m, const Cmd &cmd, const userResponseCallback &&cb);
    void onAddMonitorScraper(const Cmd &cmd, const userResponseCallback &&cb,
                             std::shared_ptr<Connection> connection);
    void onStop(MonitorOrScraper m, const Cmd &cmd,
                const userResponseCallback &&cb);
    void onStopMonitorScraper(const Cmd &cmd, const userResponseCallback &&cb,
                              std::shared_ptr<Connection> connection);
    void onGetStatus(MonitorOrScraper m, const Cmd &cmd,
                     const userResponseCallback &&cb);
    void onGetMonitorScraperStatus(const Cmd &cmd, const userResponseCallback &&cb,
                                   std::shared_ptr<Connection> connection);
};

typedef std::function<void(MonitorManager *, MonitorOrScraper m, const Cmd &cmd,
                           const userResponseCallback &&cb)>
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
