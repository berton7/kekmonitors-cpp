#include <asio.hpp>
#include <functional>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;

char buf[1024];
auto logger = kekmonitors::utils::getLogger("Stopmm");

void onRead(const error_code &err, size_t) {
    if (err && err != error::eof)
        logger->error(err.message());
    else
        logger->info(kekmonitors::utils::errorToString(kekmonitors::Response::fromString(buf).getError()));
}

void onWrite(const error_code &err, size_t,
             local::stream_protocol::socket *socket) {
    if (err) {
        logger->error(err.message());
        return;
    }
    socket->shutdown(local::stream_protocol::socket::shutdown_send);
    async_read(*socket, buffer(buf), &onRead);
}

void onConnect(const error_code &err, local::stream_protocol::socket *socket) {
    if (err) {
        logger->error(err.message());
        return;
    }
    kekmonitors::Cmd cmd;
    cmd.setCmd(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    async_write(*socket, buffer(cmd.toJson().dump()),
                std::bind(&onWrite, std::placeholders::_1,
                          std::placeholders::_2, socket));
}

int main() {
    io_context io;
    kekmonitors::init();
    local::stream_protocol::socket socket(io);
    socket.async_connect(
        local::stream_protocol::endpoint(kekmonitors::utils::getLocalKekDir() +
                                         "/sockets/MonitorManager"),
        std::bind(&onConnect, std::placeholders::_1, &socket));
    io.run();
    socket.close();
}
