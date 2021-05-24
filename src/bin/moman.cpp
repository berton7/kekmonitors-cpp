#include <kekmonitors/monitormanager.hpp>

int main() {
    boost::asio::io_context io;
    kekmonitors::init();
    kekmonitors::MonitorManager moman(io);
    io.run();
    return 0;
}