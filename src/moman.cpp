#include <kekmonitors/monitormanager.hpp>

int main()
{
	asio::io_context io;
	kekmonitors::MonitorManager moman(io);
	io.run();
	return 0;
}