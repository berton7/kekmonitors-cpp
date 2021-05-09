#pragma once
#include <asio.hpp>
#include <chrono>
#include <spdlog/logger.h>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>

using namespace asio;

typedef std::function<kekmonitors::Response(const kekmonitors::Cmd &)>
    CmdCallback;
typedef std::map<const kekmonitors::COMMANDS, CmdCallback> CallbackMap;

namespace kekmonitors {
class IConnection {
  protected:
    std::vector<char> _buffer;
    local::stream_protocol::socket _socket;
    steady_timer _timeout;

  public:
    explicit IConnection(io_context &io);
    ~IConnection();

    void onTimeout(const std::error_code &err);
    local::stream_protocol::socket &getSocket();
};

class CmdConnection : public IConnection,
                      public std::enable_shared_from_this<CmdConnection> {
  private:
    const CmdCallback _cb;
    void onRead(const std::error_code &err, size_t read);
    void onWrite(const std::error_code &err, size_t read);

  public:
    explicit CmdConnection(io_context &io, const CmdCallback &&cb);
    static std::shared_ptr<CmdConnection> create(io_context &io,
                                                 const CmdCallback &&cb);
    void asyncRead();
};

class UnixServer {
  private:
    std::unique_ptr<local::stream_protocol::acceptor> _acceptor = nullptr;
    std::string _serverPath;
    io_service &_io;
    std::shared_ptr<Config> _config = nullptr;
    std::unique_ptr<spdlog::logger> _logger = nullptr;

  public:
    CallbackMap _callbacks;

  private:
    void onConnect(const std::error_code &err,
                   std::shared_ptr<CmdConnection> &connection);

  public:
    UnixServer(io_context &io, const std::string &socketName,
               std::shared_ptr<Config> config);
    UnixServer(io_context &io, const std::string &socketName,
               std::shared_ptr<Config> config, CallbackMap callbacks);
    ~UnixServer();
    void startAccepting();
    Response _handleCallback(const Cmd &cmd);
    void shutdown();
};
} // namespace kekmonitors
