#define ASIO_HAS_IO_URING 1

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <fcntl.h>
#include <random>
#include <vector>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;
static size_t seed = 42;

asio::awaitable<void> do_reads(asio::random_access_file &file, size_t &index,
                               size_t offsets[], size_t total_blocks) {
    std::vector<char> buffer(block_size);
    while (index < total_blocks) {
        size_t current_offset = offsets[index];
        index++;
        asio::mutable_buffer buf(buffer.data(), block_size);
        co_await file.async_read_some_at(current_offset, buf,
                                         asio::use_awaitable);
    }
}

void usage(const char *prog_name) {
    std::printf(
        "Usage: %s [-h] [-b block_size] [-t num_tasks] [-s seed] <filename>\n"
        "  -h              Show this help message\n"
        "  -b block_size   Block size of each read operation in bytes\n"
        "  -t num_tasks    Number of concurrent tasks\n"
        "  -s seed         Seed for random number generator\n",
        prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:t:s:")) != -1) {
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

    size_t num_blocks = (file_size + block_size - 1) / block_size;
    std::vector<size_t> offsets(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        offsets[i] = i * block_size;
    }
    // Shuffle offsets for random read
    std::shuffle(offsets.begin(), offsets.end(), std::mt19937{seed});

    size_t index = 0;

    for (size_t i = 0; i < num_tasks; ++i) {
        asio::co_spawn(ctx, do_reads(f, index, offsets.data(), num_blocks),
                       asio::detached);
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