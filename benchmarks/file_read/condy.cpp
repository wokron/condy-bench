#include <condy.hpp>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;
static bool direct_io = false;

condy::Coro<void> do_reads(int file, size_t &offset, size_t total_size) {
    std::vector<char> buffer(block_size);
    while (offset < total_size) {
        size_t to_read = std::min(block_size, total_size - offset);
        size_t current_offset = offset;
        offset += to_read;
        auto buf = condy::buffer(buffer.data(), to_read);
        co_await condy::async_read(file, buf, current_offset);
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-hd] [-b block_size] [-t num_tasks] <filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -t num_tasks    Number of concurrent tasks\n"
                "  -d              Use direct I/O\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:t:d")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'b':
            block_size = std::stoul(optarg);
            break;
        case 't':
            num_tasks = std::stoul(optarg);
            break;
        case 'd':
            direct_io = true;
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

    int oflag = O_RDONLY;
    if (direct_io) {
        oflag |= O_DIRECT;
    }
    int file = open(filename.c_str(), oflag);

    size_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);
    size_t offset = 0;

    for (size_t i = 0; i < num_tasks; ++i) {
        condy::co_spawn(runtime, do_reads(file, offset, file_size)).detach();
    }

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughput = static_cast<double>(file_size) / elapsed.count() /
                        (1024 * 1024); // MB/s
    std::printf(
        "time_ms:%ld\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("throughput_mbps:%.2f\n", throughput);

    return 0;
}