#include <asio.hpp>
#include <iostream>
#include <kekmonitors/comms/msg.hpp>

using namespace asio;

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <cmd> [payload]" << std::endl;
		return 1;
	}
	if (argv[1] == "--list-cmd")
	{
		std::cout << "Not implemented" << std::endl;
		return 1;
	}
	kekmonitors::Cmd cmd;
	cmd.setCmd((kekmonitors::COMMANDS)std::atoi(argv[1]));
	io_context io;
	local::stream_protocol::socket sock(io);
	sock.connect(local::stream_protocol::endpoint("/home/berton/.kekmonitors/sockets/MonitorManager"));
	sock.send(buffer(cmd.toJson().dump()));
	sock.shutdown(local::stream_protocol::socket::shutdown_send);
	char buf[1024] = {};
	sock.receive(buffer(buf));
	std::cout << buf << std::endl;
	sock.close();
	return 0;
}
