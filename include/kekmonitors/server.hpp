#pragma once
#include <boost/asio.hpp>
#include <chrono>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <spdlog/logger.h>

using namespace boost::asio;

typedef std::function<void(const kekmonitors::Response &)> ResponseCallback;
typedef std::function<void(const kekmonitors::Cmd &, ResponseCallback &&)>
    CmdCallback;
typedef std::map<const kekmonitors::CommandType, CmdCallback> CallbackMap;


namespace kekmonitors {
class UnixServer;

class CmdConnection : public std::enable_shared_from_this<CmdConnection> {
  private:
    io_context &_io;
    UnixServer &_server;
    std::vector<char> _buffer;
    local::stream_protocol::socket _socket;
    steady_timer _timeout;
    void onRead(const error_code &err, size_t read);
    void onWrite(const error_code &err, size_t read);

  public:
    CmdConnection(io_context &io, UnixServer &server);
    ~CmdConnection();
    static std::shared_ptr<CmdConnection> create(io_context &io, UnixServer &server);
    void asyncRead();
    void onTimeout(const error_code &err);
    local::stream_protocol::socket &getSocket();
};

class UnixServer {
    friend CmdConnection;
  private:
    std::unique_ptr<local::stream_protocol::acceptor> _acceptor = nullptr;
    std::string _serverPath{};
    io_service &_io;
    std::shared_ptr<Config> _config = nullptr;
    std::unique_ptr<spdlog::logger> _logger = nullptr;

  public:
    CallbackMap _callbacks{};

  private:
    void onConnect(const error_code &err,
                   std::shared_ptr<CmdConnection> &connection);
    void _handleCallback(const Cmd &cmd, ResponseCallback &&responseCallback);

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
