#define ASIO_HAS_IO_URING 1

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <fcntl.h>
#include <vector>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;

asio::awaitable<void> do_reads(asio::random_access_file &file, size_t &offset,
                               size_t total_size) {
    std::vector<char> buffer(block_size);
    while (offset < total_size) {
        size_t to_read = std::min(block_size, total_size - offset);
        size_t current_offset = offset;
        offset += to_read;
        asio::mutable_buffer buf(buffer.data(), to_read);
        co_await file.async_read_some_at(current_offset, buf,
                                         asio::use_awaitable);
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-h] [-b block_size] [-t num_tasks] <filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -t num_tasks    Number of concurrent tasks\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:t:")) != -1) {
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

    asio::io_context ctx;

    asio::random_access_file f(ctx, filename,
                               asio::random_access_file::read_only);

    size_t file_size = f.size();
    size_t offset = 0;

    for (size_t i = 0; i < num_tasks; ++i) {
        asio::co_spawn(ctx, do_reads(f, offset, file_size), asio::detached);
    }

    auto start = std::chrono::high_resolution_clock::now();

    ctx.run();

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