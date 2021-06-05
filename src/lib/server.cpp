#include <boost/filesystem.hpp>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>
#include <kekmonitors/utils.hpp>
#include <utility>

using namespace boost::asio;

namespace kekmonitors {
CmdConnection::CmdConnection(io_context &io, UnixServer &server)
    : _io(io), _server(server), _buffer(1024), _socket(io), _timeout(io) {
    KDBG("Allocating new connection");
};

CmdConnection::~CmdConnection() {
    KDBG("Destroying connection");
    if (_socket.is_open())
        _socket.close();
    _timeout.cancel();
};

void CmdConnection::onTimeout(const error_code &err) {
    if (err) {
        if (err != error::operation_aborted) {
            KDBG(err.message());
            return;
        }
    } else {
        KDBG("Connection timed out");
        _socket.close();
    }
}

local::stream_protocol::socket &CmdConnection::getSocket() { return _socket; }

void CmdConnection::onRead(const error_code &err, size_t read) {
    if (!err || err == error::eof) {
        _timeout.cancel();
        try {
            auto j = json::parse(_buffer);
            error_code ec;
            auto cmd = Cmd::fromJson(j, ec);
            if (!ec) {
                auto shared = shared_from_this();
                defer(_io, [cmd, shared]() {
                    shared->_server._handleCallback(
                        cmd,
                        [cmd, shared](const Response &response) {
                            async_write(shared->_socket,
                                        buffer(response.toString()),
                                        std::bind(&CmdConnection::onWrite,
                                                  shared, std::placeholders::_1,
                                                  std::placeholders::_2));
                        },
                        shared);
                });
            } else {
                KDBG("Received connection but couldn't parse from json: " +
                     std::string(_buffer.data()));
            }
        } catch (std::exception &e) {
            KDBG(e.what());
        }
    } else if (err && err != error::operation_aborted) {
        KDBG(err.message());
    }
};

std::shared_ptr<CmdConnection> CmdConnection::create(io_context &io,
                                                     UnixServer &server) {
    return std::make_shared<CmdConnection>(io, server); // ???
}

void CmdConnection::asyncRead() {
    _timeout.expires_after(std::chrono::seconds(1));
    _timeout.async_wait(
        std::bind(&CmdConnection::onTimeout, this, std::placeholders::_1));
    async_read(_socket, buffer(_buffer),
               std::bind(&CmdConnection::onRead, shared_from_this(),
                         std::placeholders::_1, std::placeholders::_2));
}

void CmdConnection::onWrite(const error_code &err, size_t read) {
    if (err)
        KDBG(err.message());
};

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

UnixServer::~UnixServer() {
    _logger->info("Closing server");
    ::unlink(_serverPath.c_str());
};

void UnixServer::startAccepting() {
    auto connection = CmdConnection::create(_io, *this);
    _acceptor->async_accept(connection->getSocket(),
                            std::bind(&UnixServer::onConnect, this,
                                      std::placeholders::_1, connection));
};

void UnixServer::onConnect(const error_code &err,
                           std::shared_ptr<CmdConnection> &connection) {
    if (!err) {
        connection->asyncRead();
        startAccepting();
    } else if (err != error::operation_aborted)
        KDBG(err.message());
}

void UnixServer::_handleCallback(const Cmd &cmd,
                                 ResponseCallback &&responseCallback,
                                 std::shared_ptr<CmdConnection> connection) {
    auto command = static_cast<uint32_t>(cmd.getCmd());
    try {
        _logger->info("Received cmd " + utils::commandToString(cmd.getCmd()));
    } catch (std::out_of_range &) {
        _logger->info("Received cmd " +
                      std::to_string(static_cast<uint32_t>(cmd.getCmd())));
    }
    try {
        _callbacks.at(cmd.getCmd())(cmd, std::move(responseCallback),
                                    std::move(connection));
    } catch (std::out_of_range &e) {
        _logger->warn("Cmd " + std::to_string(command) + " was not registered");
        Response resp;
        resp.setError(ERRORS::UNRECOGNIZED_COMMAND);
        responseCallback(resp);
    }
}

void UnixServer::shutdown() { _acceptor->close(); }
} // namespace kekmonitors
