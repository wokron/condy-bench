#include "asio/use_awaitable.hpp"
#include <asio.hpp>

static size_t num_tasks = 1'000'000;

asio::awaitable<void> task_func() { co_return; }

asio::awaitable<void> spawner() {
    auto ex = co_await asio::this_coro::executor;
    std::vector<asio::awaitable<void>> tasks;
    for (size_t i = 0; i < num_tasks; ++i) {
        tasks.emplace_back(
            asio::co_spawn(ex, task_func(), asio::use_awaitable));
    }
    for (auto &&t : tasks) {
        co_await std::move(t);
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-h] [-n num_tasks]\n"
                "  -h            Show this help message\n"
                "  -n num_tasks  Set the number of tasks to spawn\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hn:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'n':
            num_tasks = std::stoul(optarg);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    asio::io_context io_context;

    asio::co_spawn(io_context, spawner(), asio::detached);

    auto start = std::chrono::high_resolution_clock::now();

    io_context.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::printf("time_ms:%ld\n", duration);

    return 0;
}