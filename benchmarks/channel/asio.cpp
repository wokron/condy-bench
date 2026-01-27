#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <chrono>

static size_t buffer_size = 1024;
static size_t num_messages = 1'000'000;
static size_t task_pair = 1;

using asio::as_tuple;
using asio::awaitable;
using asio::io_context;
using asio::use_awaitable;
using asio::experimental::channel;

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
        auto [ec, msg] = co_await ch.async_receive(as_tuple(use_awaitable));
        if (ec)
            break;
        ++count;
    }
    co_return;
}

void usage(const char *prog_name) {
    std::printf(
        "Usage: %s [-h] [-b buffer_size] [-n num_messages] [-p task_pair]\n"
        "  -h              Show this help message\n"
        "  -b buffer_size  Set the buffer size\n"
        "  -n num_messages Set the number of messages\n"
        "  -p task_pair    Set the task pair\n",
        prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hmb:n:p:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'b':
            buffer_size = std::stoul(optarg);
            break;
        case 'n':
            num_messages = std::stoul(optarg);
            break;
        case 'p':
            task_pair = std::stoul(optarg);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    io_context io;

    std::vector<std::unique_ptr<channel<void(asio::error_code, std::size_t)>>>
        channels;
    for (size_t i = 0; i < task_pair; ++i) {
        channels.push_back(
            std::make_unique<channel<void(asio::error_code, std::size_t)>>(
                io.get_executor(), buffer_size));
        asio::co_spawn(io, producer(*channels.back()), asio::detached);
        asio::co_spawn(io, consumer(*channels.back()), asio::detached);
    }

    auto start = std::chrono::high_resolution_clock::now();

    io.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::printf("time_ms:%ld\n", duration);

    return 0;
}