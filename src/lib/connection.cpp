//
// Created by berton on 09/07/21.
//
#include <kekmonitors/connection.hpp>

namespace kekmonitors {

Connection::Connection(io_service &io)
    : _io(io), _buffer(1024), socket(io), _timeout(io) {
    KDBG("Allocating new connection");
}

Connection::~Connection() {
    if (socket.is_open())
        socket.close();
    _timeout.cancel();
    KDBG("Connection destroyed");
}

void Connection::onTimeout(const error_code &err) {
    if (err) {
        if (err != error::operation_aborted) {
            KDBG(err.message());
            return;
        }
    } else {
        KDBG("Connection timed out");
        socket.close();
    }
}

Connection::Ptr Connection::create(io_context &io) {
    return std::make_shared<Connection>(io);
}

void Connection::asyncReadCmd(
    const CmdCallback &&cb) {
    _timeout.expires_after(std::chrono::seconds(1));
    _timeout.async_wait(
        std::bind(&Connection::onTimeout, this, std::placeholders::_1));
    auto shared = shared_from_this();
    async_read(socket, buffer(_buffer),
               [shared, this, cb](const error_code &err, size_t read) {
                   Cmd cmd;
                   if (!err || err == error::eof) {
                       _timeout.cancel();
                       error_code ec;
                       std::string buf{_buffer.begin(), _buffer.end()};
                       cmd = Cmd::fromString(buf, ec);
                       if (ec) {
                           KDBG("Received connection but couldn't parse from "
                                "json: " +
                                buf);
                       }
                       cb(ec, cmd, shared);
                   } else if (err && err != error::operation_aborted) {
                       KDBG(err.message());
                       cb(err, cmd, shared);
                   } else // operation aborted
                   {
                       cb(err, cmd, shared);
                   }
               });
}

void Connection::asyncWriteResponse(
    const Response &response,
    const std::function<void(const error_code &, Ptr)> &&cb) {
    auto shared = shared_from_this();
    async_write(socket, buffer(response.toString()),
                [shared, cb](const error_code &err, size_t read) {
                    if (err)
                        KDBG(err.message());
                    cb(err, shared);
                });
}

} // namespace kekmonitors