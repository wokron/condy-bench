#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <random>
#include <string>
#include <unistd.h>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_blocks = 1024;        // Total 1GB
static size_t seed = 42;

void do_writes(int file, std::string_view content, size_t &index,
               size_t offsets[]) {
    while (index < num_blocks) {
        size_t current_offset = offsets[index];
        ++index;
        ::pwrite(file, content.data(), block_size, current_offset);
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
    std::printf(
        "Usage: %s [-h] [-b block_size] [-n num_blocks] [-s seed] <filename>\n"
        "  -h              Show this help message\n"
        "  -b block_size   Block size of each read operation in bytes\n"
        "  -n num_blocks   Number of blocks\n"
        "  -s seed         Seed for random number generator\n",
        prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hb:n:s:")) != -1) {
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

    auto start = std::chrono::high_resolution_clock::now();

    do_writes(file, content, index, offsets.data());

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