#include <asio.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
        return 1;
    }
    if (!std::strcmp(argv[1], "--list-cmd")) {
        for (kekmonitors::CommandType i = kekmonitors::COMMANDS::PING;
             i < KEKMONITORS_FIRST_CUSTOM_COMMAND; i++) {
            std::cout << kekmonitors::utils::getStringWithoutNamespaces(
                             kekmonitors::utils::commandToString(i))
                      << "\n";
        }
        std::cout << std::endl;
        return 0;
    }
    kekmonitors::Cmd cmd;
    char *invalidPtr = nullptr;
    kekmonitors::CommandType command;
    auto intCommand =
        static_cast<uint32_t>(std::strtol(argv[1], &invalidPtr, 10));
    if (*invalidPtr != '\0') {
        try {
            command = kekmonitors::utils::stringToCommand(argv[1]);
        } catch (std::out_of_range &) {
            std::cerr << "argv[2] is not a number nor a valid command"
                      << std::endl;
            return 2;
        }
    } else
        command = static_cast<kekmonitors::CommandType>(intCommand);
    cmd.setCmd(command);
    io_context io;
    local::stream_protocol::socket sock(io);
    kekmonitors::Config cfg;
    sock.connect(local::stream_protocol::endpoint(
        cfg.parser.get<std::string>("GlobalConfig.socket_path") +
        boost::filesystem::path::separator + "MonitorManager"));
    sock.send(buffer(cmd.toJson().dump()));
    sock.shutdown(local::stream_protocol::socket::shutdown_send);
    std::vector<char> buf(1024);
    sock.receive(buffer(buf));
    sock.close();
    auto logger = kekmonitors::utils::getLogger("MomanCli");
    kekmonitors::Response resp;
    resp.fromString(std::string(buf.begin(), buf.end()));
    if (resp.getError() != kekmonitors::ERRORS::OK) {
        logger->error(kekmonitors::utils::errorToString(resp.getError()));
        if (!resp.getInfo().empty()) {
            logger->info(resp.getInfo());
        }
    } else
        logger->info(kekmonitors::utils::errorToString(resp.getError()));
    return 0;
}
