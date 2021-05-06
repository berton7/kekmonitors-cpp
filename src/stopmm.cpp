#include <asio.hpp>
#include <iostream>
#include <kekmonitors/utils.hpp>
#include <kekmonitors/comms/msg.hpp>
#include <functional>

using namespace asio;

char buf[1024];

void onRead(const error_code &err, size_t)
{
    if(err && err != error::eof)
        KERR(err.message());
    else
        KDBG(buf);
}

void onWrite(const error_code &err, size_t, local::stream_protocol::socket *socket)
{
    if(err) {
        KERR(err.message());
        return;
    }
    socket->shutdown(local::stream_protocol::socket::shutdown_send);
    async_read(*socket, buffer(buf), &onRead);
}

void onConnect(const error_code &err, local::stream_protocol::socket *socket)
{
    if(err) {
        KERR(err.message());
        return;
    }
    kekmonitors::Cmd cmd;
    cmd.setCmd(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    async_write(*socket, buffer(cmd.toJson().dump()), std::bind(&onWrite, std::placeholders::_1, std::placeholders::_2, socket));
}

int main()
{
    auto logger = kekmonitors::utils::getLogger(kekmonitors::Config(), "MonitorManager");
    logger.debug("Debug");
    logger.info("info");
    logger.warn("warn");
    logger.error("error");
    return 0;
    io_context io;
    local::stream_protocol::socket socket(io);
    socket.async_connect(local::stream_protocol::endpoint(kekmonitors::utils::getLocalKekDir() + "/sockets/MonitorManager"),std::bind(&onConnect, std::placeholders::_1, &socket));
    io.run();
    socket.close();
}
