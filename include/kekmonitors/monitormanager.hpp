#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>
#include <kekmonitors/process.hpp>

namespace kekmonitors {
class MonitorManager {
  private:
    std::unique_ptr<UnixServer> _unixServer=nullptr;
    std::shared_ptr<Config> _config=nullptr;
    std::unique_ptr<spdlog::logger> _logger = nullptr;
    std::map<std::string, std::unique_ptr<MonitorProcess>> _monitorProcesses {};


  public:
    MonitorManager() = delete;
    explicit MonitorManager(asio::io_context &io, std::shared_ptr<Config> config = nullptr);
    ~MonitorManager();
    Response shutdown();
    Response onPing(const Cmd &cmd);
    Response onAddMonitor(const Cmd &cmd);
    Response onGetMonitorStatus(const Cmd &cmd);
};
} // namespace kekmonitors
