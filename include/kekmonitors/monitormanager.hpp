#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {
class MonitorManager {
private:
  UnixServer _unixServer;
  static Response onPing(const Cmd &cmd);

public:
  MonitorManager() = delete;
  explicit MonitorManager(io_context &io);
  ~MonitorManager();
  Response shutdown();
};
} // namespace kekmonitors