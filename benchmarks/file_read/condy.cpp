#include <bits/types/struct_iovec.h>
#include <condy.hpp>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;
static bool direct_io = false;
static bool fixed = false;
static bool iopoll = false;
static bool sqpoll = false;

condy::Coro<void> do_reads(int id, char *buffer, int file, size_t &offset,
                           size_t total_size) {
    while (offset < total_size) {
        size_t to_read = std::min(block_size, total_size - offset);
        size_t current_offset = offset;
        offset += to_read;
        if (fixed) {
            auto buf = condy::fixed(id, condy::buffer(buffer, to_read));
            co_await condy::async_read(condy::fixed(0), buf, current_offset);
        } else {
            auto buf = condy::buffer(buffer, to_read);
            co_await condy::async_read(file, buf, current_offset);
        }
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-hdfpq] [-b block_size] [-t num_tasks] <filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -t num_tasks    Number of concurrent tasks\n"
                "  -d              Use direct I/O\n"
                "  -f              Use fixed fd and buffer\n"
                "  -p              Use I/O polling\n"
                "  -q              Use SQ polling\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:t:dfpq")) != -1) {
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
    // Disable periodic event checking for fair comparison with liburing bench
    options.event_interval(std::numeric_limits<size_t>::max());
    options.sq_size(num_tasks);
    if (iopoll) {
        options.enable_iopoll();
    }
    if (sqpoll) {     
        options.enable_sqpoll();
    }

    condy::Runtime runtime(options);

    int oflag = O_RDONLY;
    if (direct_io) {
        oflag |= O_DIRECT;
    }
    int file = open(filename.c_str(), oflag);

    size_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);
    size_t offset = 0;

    size_t total_buffer_size = block_size * num_tasks;
    void *data = mmap(nullptr, total_buffer_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(file);
        return 1;
    }
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

    for (size_t i = 0; i < num_tasks; ++i) {
        condy::co_spawn(runtime, do_reads(i, total_buffer + i * block_size,
                                          file, offset, file_size))
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
        "time_ms:%ld\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("throughput_mbps:%.2f\n", throughput);

    return 0;
}