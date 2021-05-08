#include <boost/filesystem.hpp>
#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/server.hpp>
#include <kekmonitors/utils.hpp>

namespace kekmonitors {
IConnection::IConnection(io_context &io)
    : _buffer(1024), _socket(io), _timeout(io){};
IConnection::~IConnection() {
    KDBGD("Destroying connection");
    if (_socket.is_open())
        _socket.close();
    _timeout.cancel();
};

void IConnection::onTimeout(const std::error_code &err) {
    if (err) {
        if (err != error::operation_aborted) {
            KWARND(err.message());
            return;
        }
    } else {
        KDBGD("Connection timed out");
        _socket.close();
    }
}

local::stream_protocol::socket &IConnection::getSocket() { return _socket; }

void CmdConnection::onRead(const std::error_code &err, size_t read) {
    if (!err || err == asio::error::eof) {
        _timeout.cancel();
        try {
            Cmd cmd;
            auto j = json::parse(_buffer);
            bool success = cmd.fromJson(j);
            if (success) {
                async_write(_socket, asio::buffer(_cb(cmd).toJson().dump()),
                            std::bind(&CmdConnection::onWrite,
                                      shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
            } else {
                KINFOD("Received connection but couldn't parse from json: " +
                       std::string(_buffer.data()));
            }
        } catch (std::exception &e) {
            KINFOD(e.what());
        }
    } else if (err && err != asio::error::operation_aborted) {
        KWARND(err.message());
    }
};

CmdConnection::CmdConnection(io_context &io, const CmdCallback &&cb)
    : IConnection(io), _cb(cb) {
    KDBGD("Allocating new connection");
}

std::shared_ptr<CmdConnection> CmdConnection::create(io_context &io,
                                                     const CmdCallback &&cb) {
    return std::make_shared<CmdConnection>(
        io, static_cast<const CmdCallback &&>(cb)); // ???
}

void CmdConnection::asyncRead() {
    _timeout.expires_after(std::chrono::seconds(1));
    _timeout.async_wait(
        std::bind(&CmdConnection::onTimeout, this, std::placeholders::_1));
    async_read(_socket, asio::buffer(_buffer),
               std::bind(&CmdConnection::onRead, shared_from_this(),
                         std::placeholders::_1, std::placeholders::_2));
}
void CmdConnection::onWrite(const error_code &err, size_t read) {
    if (err)
        KWARND(err.message());
};

std::string getServerPath(const std::shared_ptr<Config> config,
                          const std::string &socketName) {
    std::string serverPath(
        config->parser.get<std::string>("GlobalConfig.socket_path"));
    boost::filesystem::create_directories(serverPath);
    serverPath += boost::filesystem::path::separator;
    serverPath.append(socketName);
    return serverPath;
}

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       std::shared_ptr<Config> config)
    : _io(io), _config(config) {
    if (config == nullptr)
        throw std::invalid_argument("Config cannot be nullptr");
    _serverPath = getServerPath(config, socketName);
    _acceptor =
        std::make_unique<local::stream_protocol::acceptor>(io, _serverPath);
    KINFOD("Unix server initialized, accepting connections...");
    startAccepting();
};

UnixServer::UnixServer(io_context &io, const std::string &socketName,
                       std::shared_ptr<Config> config, CallbackMap callbacks)
    : _io(io), _config(config), _callbacks(std::move(callbacks)) {
    if (config == nullptr)
        throw std::invalid_argument("Config cannot be nullptr");
    _serverPath = getServerPath(config, socketName);
    _acceptor =
        std::make_unique<local::stream_protocol::acceptor>(io, _serverPath);
    KINFOD("Unix server initialized, accepting connections...");
    startAccepting();
}

UnixServer::~UnixServer() {
    KINFOD("Closing server");
    ::unlink(_serverPath.c_str());
};

void UnixServer::startAccepting() {
    auto connection =
        CmdConnection::create(_io, std::bind(&UnixServer::_handleCallback, this,
                                             std::placeholders::_1));
    _acceptor->async_accept(connection->getSocket(),
                            std::bind(&UnixServer::onConnect, this,
                                      std::placeholders::_1, connection));
};

void UnixServer::onConnect(const std::error_code &err,
                           std::shared_ptr<CmdConnection> &connection) {
    if (!err) {
        connection->asyncRead();
        startAccepting();
    } else if (err != asio::error::operation_aborted)
        KWARND(err.message());
}

Response UnixServer::_handleCallback(const Cmd &cmd) {
    auto command = static_cast<uint32_t>(cmd.getCmd());
    KINFOD("Received cmd " + std::to_string(command));
    try {
        return _callbacks.at(cmd.getCmd())(cmd);
    } catch (std::out_of_range &e) {
        KWARND("Cmd " + std::to_string(command) + " was not registered");
        Response resp;
        resp.setError(ERRORS::UNRECOGNIZED_COMMAND);
        return resp;
    }
}

void UnixServer::shutdown() { _acceptor->close(); }
} // namespace kekmonitors
