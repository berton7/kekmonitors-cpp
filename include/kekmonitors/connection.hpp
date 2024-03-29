//
// Created by berton on 09/07/21.
//

#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/steady_timer.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>

using namespace boost::asio;

namespace kekmonitors {
class Connection;
typedef std::function<void(const error_code &, const Cmd &,
                           std::shared_ptr<Connection>)>
    CmdCallback;
class Connection : public std::enable_shared_from_this<Connection> {
  public:
    typedef std::shared_ptr<Connection> Ptr;

  private:
    io_context &m_io;
    std::vector<char> m_buffer;
    steady_timer m_timeout;

    void onTimeout(const error_code &);

  public:
    local::stream_protocol::socket p_endpoint;

    explicit Connection(io_context &);
    ~Connection();
    static Ptr create(io_context &);

    void asyncReadCmd(
        const CmdCallback &&,
        const steady_timer::duration &timeout = std::chrono::seconds(1));
    void
    asyncWriteResponse(const Response &,
                       const std::function<void(const error_code &, Ptr)> &&);
    void asyncWriteCmd(const Cmd &,
                       std::function<void(const error_code &, Ptr)> &&);
    void asyncReadResponse(
        std::function<void(const error_code &, const Response &, Ptr)> &&,
        const steady_timer::duration &timeout = std::chrono::seconds(3));

    void quickWriteCmd(
        const Cmd &, std::function<void(const Response &)> &&cb,
        std::function<void(const error_code &)> on_any_error =
            [](const error_code &ec) {});
};
} // namespace kekmonitors
