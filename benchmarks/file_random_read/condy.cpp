#include <condy.hpp>
#include <random>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;
static size_t seed = 42;
static bool direct_io = false;
static bool fixed = false;
static bool iopoll = false;
static bool sqpoll = false;

condy::Coro<void> do_reads(int id, char *buffer, int file, size_t &index,
                           size_t offsets[], size_t total_blocks) {
    while (index < total_blocks) {
        size_t current_offset = offsets[index];
        index++;
        if (fixed) {
            auto buf = condy::fixed(id, condy::buffer(buffer, block_size));
            co_await condy::async_read(condy::fixed(0), buf, current_offset);
        } else {
            auto buf = condy::buffer(buffer, block_size);
            co_await condy::async_read(file, buf, current_offset);
        }
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-hdfpq] [-b block_size] [-t num_tasks] [-s seed] "
                "<filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -t num_tasks    Number of concurrent tasks\n"
                "  -s seed         Seed for random number generator\n"
                "  -d              Use direct I/O\n"
                "  -f              Use fixed file descriptor and buffer\n"
                "  -p              Use I/O polling\n"
                "  -q              Use SQ polling\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:t:s:dfpq")) != -1) {
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
        case 's':
            seed = std::stoul(optarg);
            break;
        case 'd':
            direct_io = true;
            break;
        case 'f':
            fixed = true;
            break;
        case 'p':
            iopoll = true;
            break;
        case 'q':
            sqpoll = true;
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

    condy::RuntimeOptions options;
    if (iopoll) {
        options.enable_iopoll();
    }
    if (sqpoll) {
        options.enable_sqpoll();
    }

    condy::Runtime runtime(options);

    int oflags = O_RDONLY;
    if (direct_io) {
        oflags |= O_DIRECT;
    }
    int file = open(filename.c_str(), oflags);

    size_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    size_t num_blocks = (file_size + block_size - 1) / block_size;
    std::vector<size_t> offsets(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        offsets[i] = i * block_size;
    }
    // Shuffle offsets for random read
    std::shuffle(offsets.begin(), offsets.end(), std::mt19937{seed});

    size_t total_buffer_size = block_size * num_tasks;
    void *data = mmap(nullptr, total_buffer_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *total_buffer = static_cast<char *>(data);

    if (fixed) {
        std::vector<iovec> iovecs(num_tasks);
        for (size_t i = 0; i < num_tasks; ++i) {
            iovecs[i].iov_base = total_buffer + i * block_size;
            iovecs[i].iov_len = block_size;
        }
        runtime.buffer_table().init(num_tasks);
        runtime.buffer_table().update(0, iovecs.data(), num_tasks);
        runtime.fd_table().init(1);
        runtime.fd_table().update(0, &file, 1);
    }

    size_t index = 0;

    for (size_t i = 0; i < num_tasks; ++i) {
        condy::co_spawn(runtime,
                        do_reads(i, total_buffer + i * block_size, file, index,
                                 offsets.data(), num_blocks))
            .detach();
    }

    auto start = std::chrono::high_resolution_clock::now();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double iops = static_cast<double>(num_blocks) / elapsed.count();
    std::printf(
        "time_ms:%ld\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("iops:%.2f\n", iops);

    return 0;
}