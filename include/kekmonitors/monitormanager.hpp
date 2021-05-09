#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>

namespace kekmonitors {
class MonitorManager {
  private:
    std::unique_ptr<UnixServer> _unixServer=nullptr;
    std::shared_ptr<Config> _config=nullptr;
    std::unique_ptr<spdlog::logger> _logger = nullptr;
    Response onPing(const Cmd &cmd);

  public:
    MonitorManager() = delete;
    explicit MonitorManager(asio::io_context &io, std::shared_ptr<Config> config = nullptr);
    ~MonitorManager();
    Response shutdown();
};
} // namespace kekmonitors