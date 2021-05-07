#include <iostream>
#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {

MonitorManager::MonitorManager(io_context &io)
    : _unixServer(io, "MonitorManager", Config(),
                  {{COMMANDS::PING, &MonitorManager::onPing},
                   {COMMANDS::MM_STOP_MONITOR_MANAGER,
                    std::bind(&MonitorManager::shutdown, this)}}) {}

MonitorManager::~MonitorManager() = default;

Response MonitorManager::onPing(const Cmd &cmd) {
  std::cout << "onPing callback!" << std::endl;
  return Response::okResponse();
}

Response MonitorManager::shutdown() {
  KDBGD("Received shutdown");
  _unixServer.shutdown();
  return Response::okResponse();
}
} // namespace kekmonitors
