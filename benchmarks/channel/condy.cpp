#include <chrono>
#include <condy.hpp>
#include <optional>

static size_t buffer_size = 1024;
static size_t num_messages = 1'000'000;
static size_t task_pair = 1;

condy::Coro<void> producer(condy::Channel<std::optional<int>> &ch) {
    for (int i = 0; i < num_messages; ++i) {
        co_await ch.push(i);
    }
    ch.push_close();
    co_return;
}

condy::Coro<void> consumer(condy::Channel<std::optional<int>> &ch) {
    int count = 0;
    while (true) {
        auto value = co_await ch.pop();
        if (!value.has_value())
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

    condy::Runtime runtime;

    std::vector<std::unique_ptr<condy::Channel<std::optional<int>>>> channels;
    for (size_t i = 0; i < task_pair; ++i) {
        channels.push_back(
            std::make_unique<condy::Channel<std::optional<int>>>(buffer_size));
        condy::co_spawn(runtime, producer(*channels.back())).detach();
        condy::co_spawn(runtime, consumer(*channels.back())).detach();
    }

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::printf("time_ms:%ld\n", duration);

    return 0;
}