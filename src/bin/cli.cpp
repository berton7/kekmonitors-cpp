#include <chrono>
#include <climits>
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/connection.hpp>
#include <kekmonitors/utils.hpp>
#include <stdexcept>

using namespace kekmonitors;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
        return 1;
    }
    initMaps();
    initDebugLogger();
    if (!std::strcmp(argv[1], "--list-cmd")) {
        for (CommandType i = COMMANDS::PING;
             i < KEKMONITORS_FIRST_CUSTOM_COMMAND; i++) {
            const auto commandString =
                utils::getStringWithoutNamespaces(utils::commandToString(i));
            if (commandString.substr(0, 2) == "MM")
                std::cout << commandString << "\n";
        }
        std::cout << std::endl;
        return 0;
    }

    CommandType command;
    try {
        command = utils::stringToCommand(argv[1]);
    } catch (std::out_of_range &) {
        try {
            command = static_cast<CommandType>(std::stoi(argv[1]));
        } catch (std::invalid_argument &) {
            std::cerr << "argv[2] is not a number nor a valid command"
                      << std::endl;
            return 2;
        }
    }

    Cmd cmd;
    cmd.setCmd(command);
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
    auto logger = utils::getLogger("MomanCli");
    Config cfg;
    io_context io;
    auto connection = Connection::create(io);
    connection->p_endpoint.async_connect(
        local::stream_protocol::endpoint(
            cfg.p_parser.get<std::string>("GlobalConfig.socket_path") +
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
                                        utils::errorToString(resp.error())};
                                    if (resp.error())
                                        logger->error("[Error] {}", errorStr);
                                    else
                                        logger->info("[Cmd] {}", errorStr);
                                    if (!resp.info().empty())
                                        logger->info("[Info] {}",
                                                     resp.info());
                                    if (!resp.payload().empty())
                                        logger->info("[Payload] {}",
                                                     resp.payload().dump());
                                } else
                                    logger->error(err.message());
                            }, std::chrono::seconds(10));
                    else
                        logger->error(err.message());
                });
            else
                logger->error(err.message());
        });
    io.run();
    return 0;
}
