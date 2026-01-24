#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/experimental/channel.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <iostream>

// test channel using asio with coroutines

using asio::awaitable;
using asio::io_context;
using asio::experimental::channel;
using namespace std::chrono;

constexpr int num_messages = 10000000;

awaitable<void> producer(channel<void(asio::error_code, std::size_t)> &ch) {
    for (int i = 0; i < num_messages; ++i) {
        co_await ch.async_send(asio::error_code{}, i);
    }
    ch.close();
    co_return;
}

awaitable<void> consumer(channel<void(asio::error_code, std::size_t)> &ch) {
    int count = 0;
    while (true) {
        asio::error_code ec;
        auto msg = co_await ch.async_receive();
        if (ec)
            break;
        ++count;
    }
    std::cout << "Received messages: " << count << std::endl;
    co_return;
}

int main() {
    io_context io;

    channel<void(asio::error_code, std::size_t)> ch(
        io, 1024); // channel buffer size

    auto start = high_resolution_clock::now();

    asio::co_spawn(io, producer(ch), asio::detached);
    asio::co_spawn(io, consumer(ch), asio::detached);

    io.run();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    std::cout << "Time taken: " << duration << " ms" << std::endl;

    return 0;
}