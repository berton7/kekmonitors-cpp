#include <iostream>
#include <kekmonitors/comms/msg.hpp>
#include <kekmonitors/moman.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;
using namespace std::placeholders;

namespace kekmonitors {
class MonitorManager {
  private:
    UnixServer _unixServer;
    static Response onPing(const Cmd &cmd) {
        std::cout << "onPing callback!" << std::endl;
        return Response::okResponse();
    }

  public:
    MonitorManager() = delete;
    explicit MonitorManager(io_context &io) : _unixServer(io, utils::getLocalKekDir() + "/sockets/Executable.MonitorManager") {
        _unixServer._callbacks[COMMANDS::PING] = &MonitorManager::onPing;
    }

    ~MonitorManager() = default;
};
} // namespace kekmonitors

int main() {
    io_context io;
    kekmonitors::MonitorManager m(io);
    io.run();
    return 0;
}