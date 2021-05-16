#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>
#include <kekmonitors/process.hpp>

namespace kekmonitors {
class MonitorManager {
  private:
    io_context &_io;
    std::unique_ptr<UnixServer> _unixServer=nullptr;
    std::shared_ptr<Config> _config=nullptr;
    std::unique_ptr<spdlog::logger> _logger = nullptr;
    std::unordered_map<std::string, std::shared_ptr<MonitorProcess>> _monitorProcesses {};
    std::unordered_map<std::string, std::shared_ptr<MonitorProcess>> _tmpMonitorProcesses {};

  public:
    MonitorManager() = delete;
    explicit MonitorManager(asio::io_context &io, std::shared_ptr<Config> config = nullptr);
    ~MonitorManager();
    void shutdown(const Cmd &cmd, const ResponseCallback &&cb);
    void onPing(const Cmd &cmd, const ResponseCallback &&cb);
    void onAddMonitor(const Cmd &cmd, const ResponseCallback &&cb);
    void onGetMonitorStatus(const Cmd &cmd, const ResponseCallback &&cb);
};
} // namespace kekmonitors
