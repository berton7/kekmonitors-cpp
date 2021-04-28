#pragma once
#include <asio.hpp>
#include <chrono>
#include <kekmonitors/core.hpp>
#include <kekmonitors/comms/msg.hpp>

using namespace asio;
using namespace std::chrono;

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
    local::stream_protocol::acceptor _acceptor;
    const std::string _serverPath;
    io_service &_io;

  public:
    CallbackMap _callbacks;

  private:
    void onConnect(const std::error_code &err,
                   std::shared_ptr<CmdConnection> &connection);

  public:
    UnixServer(io_context &io, std::string serverPath);
    UnixServer(io_context &io, std::string serverPath, CallbackMap callbacks);
    ~UnixServer();
    void startAccepting();
    Response _handleCallback(const Cmd &cmd);
    void shutdown();
};
} // namespace kekmonitors

