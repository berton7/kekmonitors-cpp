#include <asio.hpp>
#include <iostream>
#include <kekmonitors/msg.hpp>

using namespace asio;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
    return 1;
  }
  if (!std::strcmp(argv[1], "--list-cmd")) {
    std::cerr << "Not implemented" << std::endl;
    return 1;
  }
  kekmonitors::Cmd cmd;
  char *invalidPtr = nullptr;
  int val = (int)std::strtol(argv[1], &invalidPtr, 10);
  if (*invalidPtr != '\0') {
    std::cerr << "Not a number: " << invalidPtr << std::endl;
    return 2;
  }
  cmd.setCmd((kekmonitors::COMMANDS)val);
  io_context io;
  local::stream_protocol::socket sock(io);
  sock.connect(local::stream_protocol::endpoint(
      "/home/berton/.kekmonitors/sockets/MonitorManager"));
  sock.send(buffer(cmd.toJson().dump()));
  sock.shutdown(local::stream_protocol::socket::shutdown_send);
  char buf[1024] = {};
  sock.receive(buffer(buf));
  std::cout << buf << std::endl;
  sock.close();
  return 0;
}
