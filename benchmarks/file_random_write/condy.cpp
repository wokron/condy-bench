#include <condy.hpp>
#include <fcntl.h>
#include <random>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_blocks = 1024;        // Total 1GB
static size_t num_tasks = 32;
static size_t seed = 42;

condy::Coro<void> do_writes(int file, std::string_view content, size_t &index,
                            size_t offsets[]) {
    while (index < num_blocks) {
        size_t current_offset = offsets[index];
        ++index;
        auto buf = condy::buffer(content.data(), block_size);
        co_await condy::async_write(file, buf, current_offset);
    }
}

std::string generate_content(size_t size) {
    std::string content(size, 0);
    for (size_t i = 0; i < size; ++i) {
        content[i] = 'A' + (i % 26);
    }
    return content;
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-h] [-b block_size] [-n num_blocks] [-t num_tasks] "
                "[-s seed] <filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -n num_blocks   Number of blocks\n"
                "  -t num_tasks    Number of concurrent tasks\n"
                "  -s seed         Seed for random number generator\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:n:t:s:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'b':
            block_size = std::stoul(optarg);
            break;
        case 'n':
            num_blocks = std::stoul(optarg);
            break;
        case 't':
            num_tasks = std::stoul(optarg);
            break;
        case 's':
            seed = std::stoul(optarg);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
        return 1;
    }

    std::string filename = argv[optind];

    condy::Runtime runtime;

    int file = open(filename.c_str(), O_CREAT | O_WRONLY, 0644);

    size_t file_size = block_size * num_blocks;

    std::string content = generate_content(block_size);

    std::vector<size_t> offsets(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        offsets[i] = i * block_size;
    }
    // Shuffle offsets for random write
    std::shuffle(offsets.begin(), offsets.end(), std::mt19937{seed});

    size_t index = 0;

    for (size_t i = 0; i < num_tasks; ++i) {
        condy::co_spawn(runtime,
                        do_writes(file, content, index, offsets.data()))
            .detach();
    }

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughput = static_cast<double>(file_size) / elapsed.count() /
                        (1024 * 1024); // MB/s
    std::printf(
        "time:%ldms\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("throughput:%.2f MB/s\n", throughput);

    return 0;
}