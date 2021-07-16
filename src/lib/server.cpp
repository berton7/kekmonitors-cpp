#include <boost/filesystem.hpp>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>
#include <kekmonitors/utils.hpp>
#include <utility>

using namespace boost::asio;

namespace kekmonitors {

std::string getServerPath(const std::shared_ptr<Config> &config,
                          const std::string &socketName) {
    std::string serverPath{
        config->parser.get<std::string>("GlobalConfig.socket_path")};
    boost::filesystem::create_directories(serverPath);
    serverPath += boost::filesystem::path::separator;
    serverPath.append(socketName);
    return serverPath;
}

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       std::shared_ptr<Config> config)
    : _io(io), _config(std::move(config)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("UnixServer");
    _serverPath = getServerPath(_config, socketName);
    _acceptor =
        std::make_unique<local::stream_protocol::acceptor>(_io, _serverPath);
    _logger->info("Unix server initialized, accepting connections...");
    startAccepting();
};

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       CallbackMap callbacks, std::shared_ptr<Config> config)
    : _io(io), _config(std::move(config)), _callbacks(std::move(callbacks)) {
    if (!_config)
        _config = std::make_shared<Config>();
    _logger = utils::getLogger("UnixServer");
    _serverPath = getServerPath(_config, socketName);
    _acceptor =
        std::make_unique<local::stream_protocol::acceptor>(_io, _serverPath);
    _logger->info("Unix server initialized, accepting connections...");
    startAccepting();
}

UnixServer::~UnixServer(){};

void UnixServer::startAccepting() {
    auto connection = Connection::create(_io);
    _acceptor->async_accept(connection->socket,
                            std::bind(&UnixServer::onConnect, this,
                                      std::placeholders::_1, connection));
};

void UnixServer::onConnect(const error_code &err,
                           std::shared_ptr<Connection> &connection) {
    if (!err) {
        connection->asyncReadCmd(
            std::bind(&UnixServer::_handleCallback, this, std::placeholders::_1,
                      std::placeholders::_2, connection));
        startAccepting();
    } else if (err != error::operation_aborted)
        KDBG(err.message());
}

void UnixServer::_handleCallback(const error_code &err, const Cmd &cmd,
                                 std::shared_ptr<Connection> connection) {
    if (err) {
        KDBG("Error: " + err.message());
        return;
    }
    auto command = static_cast<uint32_t>(cmd.getCmd());
    try {
        _logger->info("Received cmd " + utils::commandToString(cmd.getCmd()));
    } catch (std::out_of_range &) {
        _logger->info("Received cmd " +
                      std::to_string(static_cast<uint32_t>(cmd.getCmd())));
    }
    try {
        _callbacks.at(cmd.getCmd())(
            cmd,
            std::bind(&Connection::asyncWriteResponse, connection,
                      std::placeholders::_1,
                      [](const error_code &, Connection::Ptr) {}),
            connection);
    } catch (std::out_of_range &e) {
        _logger->warn("Cmd " + std::to_string(command) + " was not registered");
        Response resp;
        resp.setError(ERRORS::UNRECOGNIZED_COMMAND);
        connection->asyncWriteResponse(
            resp, [](const error_code &, Connection::Ptr) {});
    }
}

void UnixServer::shutdown() {
    _logger->info("Closing server");
    _acceptor->close();
    ::unlink(_serverPath.c_str());
}
} // namespace kekmonitors
