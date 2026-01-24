#include <condy.hpp>

static size_t num_tasks = 1'000'000;

condy::Coro<void> task_func() { co_return; }

condy::Coro<void> spawner() {
    std::vector<condy::Task<void>> tasks;
    for (size_t i = 0; i < num_tasks; ++i) {
        tasks.emplace_back(condy::co_spawn(task_func()));
    }
    for (auto &t : tasks) {
        co_await t;
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

    condy::Runtime runtime;

    condy::co_spawn(runtime, spawner()).detach();

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::printf("time:%ldms\n", duration);

    return 0;
}