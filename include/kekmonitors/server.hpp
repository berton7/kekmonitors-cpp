#pragma once
#include <boost/asio.hpp>
#include <chrono>
#include <kekmonitors/config.hpp>
#include <kekmonitors/connection.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <spdlog/logger.h>

using namespace boost::asio;

namespace kekmonitors {

typedef std::function<void(const kekmonitors::Response &, Connection::Ptr)>
    UserResponseCallback;
typedef std::function<void(const kekmonitors::Cmd &, UserResponseCallback &&,
                           Connection::Ptr)>
    userCmdCallback;
typedef std::map<const kekmonitors::CommandType, userCmdCallback> CallbackMap;

class UnixServer {
  private:
    std::unique_ptr<local::stream_protocol::acceptor> _acceptor{nullptr};
    std::string _serverPath{};
    io_service &_io;
    std::shared_ptr<Config> _config{nullptr};
    std::unique_ptr<spdlog::logger> _logger{nullptr};

  public:
    CallbackMap _callbacks{};

  private:
    void onConnect(const error_code &err,
                   std::shared_ptr<Connection> &connection);
    void _handleCallback(const error_code &, const Cmd &cmd, Connection::Ptr);

  public:
    UnixServer(io_context &io, const std::string &socketName,
               std::shared_ptr<Config> config = nullptr);
    UnixServer(io_context &io, const std::string &socketName,
               CallbackMap callbacks, std::shared_ptr<Config> config = nullptr);
    ~UnixServer();
    void startAccepting();
    void shutdown();
};
} // namespace kekmonitors
