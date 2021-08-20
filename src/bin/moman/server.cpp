#include "server.hpp"
#include <boost/asio/error.hpp>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <utility>

using namespace boost::asio;

namespace kekmonitors {

std::string getServerPath(const std::string &socketName) {
    std::string serverPath{
        getConfig().p_parser.get<std::string>("GlobalConfig.socket_path")};
    fs::create_directories(serverPath);
    serverPath += "/" + socketName;
    return serverPath;
}

UnixServer::UnixServer(io_context &io, const std::string &socketName)
    : UnixServer(io, socketName, {}){};

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       const CallbackMap &callbacks)
    : m_io(io), p_callbacks(callbacks) {
    m_logger = utils::getLogger("UnixServer");
    m_serverPath = getServerPath(socketName);
#ifdef KEKMONITORS_DEBUG
    KDBG("Removing leftover monitor manager socket. Beware this is a "
         "debug-only feature.");
    ::unlink(m_serverPath.c_str());
#endif
    m_acceptor =
        std::make_unique<local::stream_protocol::acceptor>(m_io, m_serverPath);
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
    } else {
        if (err != error::operation_aborted && m_acceptor->is_open()) {
            m_logger->error("Error while accepting connection: {}",
                            err.message());
        } else if (m_acceptor->is_open())
            startAccepting();
    }
}

void UnixServer::_handleCallback(const error_code &err, const Cmd &cmd,
                                 std::shared_ptr<Connection> connection) {
    if (err) {
        if (err != error::operation_aborted)
            m_logger->error("Error while reading CMD: {}", err.message());
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
        m_logger->warn("Cmd " + std::to_string(command) +
                       " was not registered");
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

void UnixServer::setServerPath(const std::string &socketName) {
    m_serverPath = getServerPath(socketName);
}

std::string &UnixServer::serverPath() { return m_serverPath; }
} // namespace kekmonitors
