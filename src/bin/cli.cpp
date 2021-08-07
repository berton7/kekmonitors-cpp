#include "kekmonitors/connection.hpp"
#include "kekmonitors/typedefs.hpp"
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/utils.hpp>

using namespace kekmonitors;

int main(int argc, char *argv[]) {
    Cmd cmd;
    char *invalidPtr = nullptr;
    CommandType command;
    auto intCommand = 0;
    //    static_cast<uint32_t>(std::strtol(argv[1], &invalidPtr, 10));
    goto TEST;
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
        return 1;
    }
    init();
    if (!std::strcmp(argv[1], "--list-cmd")) {
        for (CommandType i = COMMANDS::PING;
             i < KEKMONITORS_FIRST_CUSTOM_COMMAND; i++) {
            std::cout << utils::getStringWithoutNamespaces(
                             utils::commandToString(i))
                      << "\n";
        }
        std::cout << std::endl;
        return 0;
    }
    if (*invalidPtr != '\0') {
        try {
            command = utils::stringToCommand(argv[1]);
        } catch (std::out_of_range &) {
            std::cerr << "argv[2] is not a number nor a valid command"
                      << std::endl;
            return 2;
        }
    } else
        command = static_cast<CommandType>(intCommand);

    if (argc > 2) {
        json payload;
        if (!(argc % 2)) {
            for (int i = 2; i < argc; i++) {
                if (!(i % 2)) {
                    if (!(argv[i][0] == '-' && argv[i][1] == '-')) {
                        std::cerr
                            << "You must start every payload key with \"--\""
                            << std::endl;
                        return 4;
                    }
                    payload[argv[i] + 2] = argv[i + 1];
                }
            }
        } else {
            std::cerr << "Incorrect number of payload options!" << std::endl;
            return 3;
        }
        cmd.setPayload(payload);
    }
    cmd.setCmd(command);
TEST:
    cmd.setCmd(COMMANDS::MM_ADD_MONITOR);
    cmd.setPayload({{"name", "Asd"}});
    auto logger = utils::getLogger("MomanCli");
    init();
    Config cfg;
    io_context io;
    auto connection = Connection::create(io);
    connection->socket.async_connect(
        local::stream_protocol::endpoint(
            cfg.parser.get<std::string>("GlobalConfig.socket_path") +
            "/MonitorManager"),
        [=](const error_code &err) {
            if (!err)
                connection->asyncWriteCmd(cmd, [=](const error_code &err,
                                                   Connection::Ptr connection) {
                    if (!err)
                        connection->asyncReadResponse(
                            [=](const error_code &err, const Response resp,
                                Connection::Ptr connection) {
                                if (!err) {
                                    std::string errorStr{
                                        utils::errorToString(resp.getError())};
                                    if (resp.getError())
                                        logger->error(errorStr);
                                    else
                                        logger->info(errorStr);
                                    if (!resp.getInfo().empty())
                                        logger->info(resp.getInfo());
                                    if (!resp.getPayload().empty())
                                        logger->info(resp.getPayload().dump());
                                }
                                else
                                    logger->error(err.message());
                            });
                    else
                        logger->error(err.message());
                });
            else
                logger->error(err.message());
        });
    io.run();
    return 0;
}
