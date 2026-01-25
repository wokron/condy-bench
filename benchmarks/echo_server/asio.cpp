#include <asio.hpp>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;

constexpr size_t MAX_MESSAGE_LEN = 2048;

awaitable<void> session(tcp::socket socket) {
    char data[MAX_MESSAGE_LEN];
    for (;;) {
        std::size_t n =
            co_await socket.async_read_some(asio::buffer(data), use_awaitable);
        if (n == 0) {
            // Connection closed by client
            break;
        }
        co_await asio::async_write(socket, asio::buffer(data, n),
                                   use_awaitable);
    }
}

awaitable<void> listener(std::string host, uint16_t port) {
    auto executor = co_await this_coro::executor;
    tcp::endpoint endpoint(asio::ip::make_address(host), port);
    tcp::acceptor acceptor(executor, endpoint);

    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
        co_spawn(executor, session(std::move(socket)), detached);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::printf("Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }
    std::string host = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    asio::io_context ctx(1);
    co_spawn(ctx, listener(host, port), detached);
    std::printf("Echo server listening on %s:%d\n", host.c_str(), port);
    ctx.run();

    return 0;
}