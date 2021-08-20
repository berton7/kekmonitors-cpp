#include <functional>
#include <iostream>
#include <kekmonitors/connection.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

int main() {
    io_context io;
    kekmonitors::init();
    auto logger = kekmonitors::utils::getLogger("Stopmm");
    auto connection = kekmonitors::Connection::create(io);
    try {
        connection->p_endpoint.connect(local::stream_protocol::endpoint(
            kekmonitors::utils::getLocalKekDir() + "/sockets/MonitorManager"));
    } catch (std::exception &) {
        logger->error("Couldn't connect to socket");
        return 1;
    }
    kekmonitors::Cmd cmd;
    cmd.setCmd(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    connection->asyncWriteCmd(
        cmd, [logger](const kekmonitors::error_code &ec,
                      kekmonitors::Connection::Ptr connection) {
            if (ec) {
                logger->error(ec.message());
                return;
            }
            connection->asyncReadResponse(
                [logger](const kekmonitors::error_code &ec,
                         const kekmonitors::Response &resp,
                         kekmonitors::Connection::Ptr) {
                    if (ec) {
                        logger->error(ec.message());
                        return;
                    }
                    kekmonitors::ErrorType e = resp.error();
                    if ((e = resp.error())) {
                        logger->error(
                            kekmonitors::utils::errorToString(resp.error()));
                        logger->error(resp.info());
                    } else {
                        logger->info(
                            kekmonitors::utils::errorToString(resp.error()));
                    }
                });
        });
    io.run();
}
