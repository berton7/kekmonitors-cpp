#include <asio.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

using namespace asio;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
        return 1;
    }
    if (!std::strcmp(argv[1], "--list-cmd")) {
        for (const auto &commandPair: kekmonitors::utils::commandStringMap.left)
        {
            std::cout << commandPair.second << "\n";
        }
        std::cout << std::endl;
        return 0;
    }
    kekmonitors::Cmd cmd;
    char *invalidPtr = nullptr;
    int val = (int)std::strtol(argv[1], &invalidPtr, 10);
    if (*invalidPtr != '\0') {
        std::cerr << "Not a number: " << invalidPtr << std::endl;
        return 2;
    }
    cmd.setCmd((kekmonitors::COMMANDS)val);
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
