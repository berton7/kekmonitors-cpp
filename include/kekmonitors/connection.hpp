//
// Created by berton on 09/07/21.
//

#pragma once
#include <boost/asio.hpp>
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
    io_context &_io;
    std::vector<char> _buffer;
    steady_timer _timeout;

    void onTimeout(const error_code &);

  public:
    local::stream_protocol::socket socket;

    explicit Connection(io_context &);
    ~Connection();
    static Ptr create(io_context &);

    void asyncReadCmd(const CmdCallback &&);
    void
    asyncWriteResponse(const Response &,
                       const std::function<void(const error_code &, Ptr)> &&);
    void asyncWriteCmd(const Cmd &,
                       std::function<void(const error_code &, Ptr)> &&);
    void asyncReadResponse(
        std::function<void(const error_code &, const Response &, Ptr)> &&);
};
} // namespace kekmonitors