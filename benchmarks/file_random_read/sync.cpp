#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fcntl.h>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

static size_t block_size = 1024 * 1024; // 1MB
static size_t seed = 42;

void do_reads(int file, size_t &index, size_t offsets[], size_t total_blocks) {
    std::vector<char> buffer(block_size);
    while (index < total_blocks) {
        size_t current_offset = offsets[index];
        index++;
        ::pread(file, buffer.data(), block_size, current_offset);
    }
}

void usage(const char *prog_name) {
    std::printf("Usage: %s [-h] [-b block_size] [-s seed] <filename>\n"
                "  -h              Show this help message\n"
                "  -b block_size   Block size of each read operation in bytes\n"
                "  -s seed         Seed for random number generator\n",
                prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:s:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'b':
            block_size = std::stoul(optarg);
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

    int file = open(filename.c_str(), O_RDONLY);

    size_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    size_t num_blocks = (file_size + block_size - 1) / block_size;
    std::vector<size_t> offsets(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        offsets[i] = i * block_size;
    }
    // Shuffle offsets for random read
    std::shuffle(offsets.begin(), offsets.end(), std::mt19937{seed});

    auto start = std::chrono::high_resolution_clock::now();

    size_t index = 0;
    do_reads(file, index, offsets.data(), num_blocks);

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