//
// Created by berton on 09/07/21.
//
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <kekmonitors/connection.hpp>

namespace kekmonitors {

Connection::Connection(io_context &io)
    : m_io(io), m_buffer(1024), p_endpoint(io), m_timeout(io) {
    KDBG("Allocating new connection");
}

Connection::~Connection() {
    if (p_endpoint.is_open())
        p_endpoint.close();
    m_timeout.cancel();
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
        p_endpoint.close();
    }
}

Connection::Ptr Connection::create(io_context &io) {
    return std::make_shared<Connection>(io);
}

void Connection::asyncReadCmd(const CmdCallback &&cb,
                              const steady_timer::duration &timeout) {
    m_timeout.expires_after(timeout);
    m_timeout.async_wait(std::bind(&Connection::onTimeout, this, ph::_1));
    auto shared = shared_from_this();
    async_read(p_endpoint, buffer(m_buffer),
               [shared, this, cb](const error_code &err, size_t read) {
                   Cmd cmd;
                   if (!err || err == error::eof) {
                       m_timeout.cancel();
                       error_code ec;
                       std::string buf{m_buffer.begin(), m_buffer.end()};
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
    async_write(p_endpoint, buffer(response.toString()),
                [shared, cb](const error_code &err, size_t read) {
                    if (err)
                        KDBG(err.message());
                    shared->p_endpoint.shutdown(
                        local::stream_protocol::socket::shutdown_send);
                    cb(err, shared);
                });
}

void Connection::asyncWriteCmd(
    const Cmd &cmd, std::function<void(const error_code &, Ptr)> &&cb) {
    auto shared = shared_from_this();
    async_write(p_endpoint, buffer(cmd.toString()),
                [shared, cb](const error_code &err, size_t read) {
                    if (err)
                        KDBG(err.message());
                    shared->p_endpoint.shutdown(
                        local::stream_protocol::socket::shutdown_send);
                    cb(err, shared);
                });
}

void Connection::asyncReadResponse(
    std::function<void(const error_code &, const Response &, Ptr)> &&cb,
    const steady_timer::duration &timeout) {
    m_timeout.expires_after(timeout);
    m_timeout.async_wait(std::bind(&Connection::onTimeout, this, ph::_1));
    auto shared = shared_from_this();
    async_read(p_endpoint, buffer(m_buffer),
               [shared, this, cb](const error_code &err, size_t read) {
                   Response response;
                   if (!err || err == error::eof) {
                       m_timeout.cancel();
                       error_code ec;
                       std::string buf{m_buffer.begin(), m_buffer.end()};
                       response = Response::fromString(buf, ec);
                       if (ec) {
                           KDBG("Received connection but couldn't parse from "
                                "json: " +
                                buf);
                       }
                       cb(ec, response, shared);
                   } else if (err && err != error::operation_aborted) {
                       KDBG(err.message());
                       cb(err, response, shared);
                   } else // operation aborted
                   {
                       cb(err, response, shared);
                   }
               });
}

} // namespace kekmonitors