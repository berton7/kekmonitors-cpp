#include <asio.hpp>
#include <functional>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace asio;

char buf[1024];
spdlog::logger logger("Stopmm");

void onRead(const error_code &err, size_t) {
    if (err && err != error::eof)
        logger.error(err.message());
    else
        logger.info(buf);
}

void onWrite(const error_code &err, size_t,
             local::stream_protocol::socket *socket) {
    if (err) {
        logger.error(err.message());
        return;
    }
    socket->shutdown(local::stream_protocol::socket::shutdown_send);
    async_read(*socket, buffer(buf), &onRead);
}

void onConnect(const error_code &err, local::stream_protocol::socket *socket) {
    if (err) {
        logger.error(err.message());
        return;
    }
    kekmonitors::Cmd cmd;
    cmd.setCmd(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    async_write(*socket, buffer(cmd.toJson().dump()),
                std::bind(&onWrite, std::placeholders::_1,
                          std::placeholders::_2, socket));
}

int main() {
    logger.sinks() = {std::make_shared<spdlog::sinks::stdout_color_sink_st>()};
    logger.set_level(spdlog::level::debug);
    io_context io;
    local::stream_protocol::socket socket(io);
    socket.async_connect(
        local::stream_protocol::endpoint(kekmonitors::utils::getLocalKekDir() +
                                         "/sockets/MonitorManager"),
        std::bind(&onConnect, std::placeholders::_1, &socket));
    io.run();
    socket.close();
}
