#include <condy.hpp>
#include <cstddef>

static size_t num = 50'000'000;

condy::Coro<void> test() {
    for (size_t i = 0; i < num; i++) {
        co_await condy::co_switch(condy::current_runtime());
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-h] [-n num]\n"
                "  -h            Show this help message\n"
                "  -n num        Set the number of operations to perform\n",
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
            num = std::stoul(optarg);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }
    condy::Runtime runtime;
    condy::co_spawn(runtime, test()).detach();

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::printf("time:%ldms\n", duration);
}