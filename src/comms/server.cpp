#include <iostream>
#include <kekmonitors/comms/msg.hpp>
#include <kekmonitors/comms/server.hpp>
#include <kekmonitors/utils.hpp>

using namespace std::placeholders;

namespace kekmonitors {
IConnection::IConnection(io_context &io)
    : _buffer(1024), _socket(io), _timeout(io){};
IConnection::~IConnection() {
    KDBG("Destroying connection");
    if (_socket.is_open())
        _socket.close();
    _timeout.cancel();
};

void IConnection::onTimeout(const std::error_code &err) {
    if (err) {
        if (err != error::operation_aborted) {
            KWARN(err.message());
            return;
        }
    } else {
        KDBG("Connection timed out");
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
                                      shared_from_this(), _1, _2));
            } else {
                KINFO("Received connection but couldn't parse from json: " + std::string(_buffer.data()));
            }
        } catch (std::exception &e) {
            KINFO(e.what());
        }
    } else if (err && err != asio::error::operation_aborted) {
        KWARN(err.message());
    }
};

CmdConnection::CmdConnection(io_context &io, const CmdCallback &&cb)
    : IConnection(io), _cb(cb) {
    KDBG("Allocating new connection");
}

std::shared_ptr<CmdConnection> CmdConnection::create(io_context &io,
                                                     const CmdCallback &&cb) {
    return std::make_shared<CmdConnection>(
        io, static_cast<const CmdCallback &&>(cb)); // ???
}

void CmdConnection::asyncRead() {
    _timeout.expires_after(std::chrono::seconds(1));
    _timeout.async_wait(std::bind(&CmdConnection::onTimeout, this, _1));
    async_read(_socket,
        asio::buffer(_buffer),
        std::bind(&CmdConnection::onRead, shared_from_this(), _1, _2));
}
void CmdConnection::onWrite(const error_code &err, size_t read) {
    if (err)
        KWARN(err.message());
};

UnixServer::UnixServer(io_context &io, std::string serverPath)
    : _io(io), _serverPath(std::move(serverPath)),
      _acceptor(io, local::stream_protocol::endpoint(serverPath), true) {
    startAccepting();
};

UnixServer::UnixServer(io_context &io, std::string serverPath, CallbackMap callbacks): _io(io), _serverPath(std::move(serverPath)),
                                                                                        _acceptor(io, local::stream_protocol::endpoint(serverPath), true), _callbacks(std::move(callbacks)){
    startAccepting();
}

UnixServer::~UnixServer() {
    KINFO("Closing server");
    ::unlink(_serverPath.c_str());
};

void UnixServer::startAccepting() {
    auto connection = CmdConnection::create(
        _io, std::bind(&UnixServer::_handleCallback, this, _1));
    _acceptor.async_accept(
        connection->getSocket(),
        std::bind(&UnixServer::onConnect, this, _1, connection));
};

void UnixServer::onConnect(const std::error_code &err,
                           std::shared_ptr<CmdConnection> &connection) {
    if (!err) {
        connection->asyncRead();
        startAccepting();
    } else if (err != asio::error::operation_aborted)
        KWARN(err.message());
}

Response UnixServer::_handleCallback(const Cmd &cmd) {
    std::cout << "Received cmd: " << cmd.getCmd() << std::endl;
    try {
        return _callbacks.at(cmd.getCmd())(cmd);
    } catch (std::out_of_range &e) {
        return Response::badResponse();
    }
}

void UnixServer::shutdown() { _acceptor.close(); }
} // namespace kekmonitors
