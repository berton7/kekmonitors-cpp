#include <kekmonitors/monitormanager.hpp>
#include <kekmonitors/utils.hpp>

int main()
{
    kekmonitors::utils::initDebugLogger();
    asio::io_context io;
    kekmonitors::MonitorManager moman(io);
    io.run();
    return 0;
}