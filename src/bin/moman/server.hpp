#pragma once
#include <boost/asio/io_context.hpp>
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
    std::unique_ptr<local::stream_protocol::acceptor> m_acceptor{nullptr};
    std::string m_serverPath{};
    io_context &m_io;
    std::shared_ptr<spdlog::logger> m_logger{nullptr};

  public:
    CallbackMap p_callbacks{};

  private:
    void onConnect(const error_code &err,
                   std::shared_ptr<Connection> &connection);
    void _handleCallback(const error_code &, const Cmd &cmd, Connection::Ptr);

  public:
    UnixServer(io_context &io, const std::string &socketName);
    UnixServer(io_context &io, const std::string &socketName,
               const CallbackMap &callbacks);
    ~UnixServer();
    void startAccepting();
    void shutdown();

    void setServerPath(const std::string &socketName);
    std::string &serverPath();
};
} // namespace kekmonitors
