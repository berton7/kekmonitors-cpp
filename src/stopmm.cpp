#include <asio.hpp>
#include <iostream>
#include <kekmonitors/utils.hpp>

using namespace asio;
using namespace std::placeholders;

char buf[1024];

void onRead(const error_code &err, size_t)
{
    if(err && err != error::eof)
        std::cerr << "onRead: " << err.message() << std::endl;
    else
        std::cout << buf << std::endl;
}

void onWrite(const error_code &err, size_t, local::stream_protocol::socket *socket)
{
    if(err) {
        std::cerr << "onWrite: " << err.message() << std::endl;
        return;
    }
    socket->shutdown(local::stream_protocol::socket::shutdown_send);
    async_read(*socket, buffer(buf), &onRead);
}

void onConnect(const error_code &err, local::stream_protocol::socket *socket)
{
    if(err) {
        std::cerr << "onWrite: " << err.message() << std::endl;
        return;
    }
    async_write(*socket, buffer("{\"_Cmd__cmd\": 1}"), std::bind(&onWrite, _1, _2, socket));
}

int main()
{
    io_context io;
    local::stream_protocol::socket socket(io);
    socket.async_connect(local::stream_protocol::endpoint(kekmonitors::utils::getLocalKekDir() + "/sockets/MonitorManager"),std::bind(&onConnect, _1, &socket));
    io.run();
    socket.close();
}