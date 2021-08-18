#include "server.hpp"
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <utility>

using namespace boost::asio;

namespace kekmonitors {

std::string getServerPath(const std::shared_ptr<Config> &config,
                          const std::string &socketName) {
    std::string serverPath{
        config->p_parser.get<std::string>("GlobalConfig.socket_path")};
    fs::create_directories(serverPath);
    serverPath += "/" + socketName;
    return serverPath;
}

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       std::shared_ptr<Config> config)
    : m_io(io), m_config(std::move(config)) {
    if (!m_config)
        m_config = std::make_shared<Config>();
    m_logger = utils::getLogger("UnixServer");
    m_serverPath = getServerPath(m_config, socketName);
    m_acceptor =
        std::make_unique<local::stream_protocol::acceptor>(m_io, m_serverPath);
    m_logger->info("Unix server initialized, accepting connections...");
    startAccepting();
};

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       CallbackMap callbacks, std::shared_ptr<Config> config)
    : m_io(io), m_config(std::move(config)), p_callbacks(std::move(callbacks)) {
    if (!m_config)
        m_config = std::make_shared<Config>();
    m_logger = utils::getLogger("UnixServer");
    m_serverPath = getServerPath(m_config, socketName);
    m_acceptor =
        std::make_unique<local::stream_protocol::acceptor>(m_io, m_serverPath);
    m_logger->info("Unix server initialized, accepting connections...");
    startAccepting();
}

UnixServer::~UnixServer(){};

void UnixServer::startAccepting() {
    auto connection = Connection::create(m_io);
    m_acceptor->async_accept(
        connection->p_socket,
        std::bind(&UnixServer::onConnect, this, ph::_1, connection));
};

void UnixServer::onConnect(const error_code &err,
                           std::shared_ptr<Connection> &connection) {
    if (!err) {
        connection->asyncReadCmd(std::bind(&UnixServer::_handleCallback, this,
                                           ph::_1, ph::_2, connection));
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
    auto command = static_cast<uint32_t>(cmd.cmd());
    try {
        m_logger->info("Received cmd " + utils::commandToString(cmd.cmd()));
    } catch (std::out_of_range &) {
        m_logger->info("Received cmd " +
                      std::to_string(static_cast<uint32_t>(cmd.cmd())));
    }
    try {
        p_callbacks.at(cmd.cmd())(
            cmd,
            std::bind(&Connection::asyncWriteResponse, connection, ph::_1,
                      [](const error_code &, Connection::Ptr) {}),
            connection);
    } catch (std::out_of_range &e) {
        m_logger->warn("Cmd " + std::to_string(command) + " was not registered");
        Response resp;
        resp.setError(ERRORS::UNRECOGNIZED_COMMAND);
        connection->asyncWriteResponse(
            resp, [](const error_code &, Connection::Ptr) {});
    }
}

void UnixServer::shutdown() {
    m_logger->info("Closing server");
    m_acceptor->close();
    ::unlink(m_serverPath.c_str());
}
} // namespace kekmonitors
