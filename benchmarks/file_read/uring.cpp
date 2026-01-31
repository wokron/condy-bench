#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <liburing.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t block_size = 1024 * 1024; // 1MB
static size_t num_tasks = 32;

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

    int file = open(filename.c_str(), O_RDONLY | O_DIRECT);
    if (file < 0) {
        perror("open");
        return 1;
    }

    size_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    size_t total_buffer_size = block_size * num_tasks;
    void *data = mmap(nullptr, total_buffer_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(file);
        return 1;
    }
    char *total_buffer = static_cast<char *>(data);

    int r;
    io_uring ring;
    if ((r = io_uring_queue_init(num_tasks, &ring,
                                 IORING_SETUP_SINGLE_ISSUER)) < 0) {
        std::fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-r));
        munmap(data, total_buffer_size);
        close(file);
        return 1;
    }

    size_t offset = 0;
    size_t left = file_size;

    io_uring_sqe *sqe;

    auto start = std::chrono::high_resolution_clock::now();

    // Launch initial read requests
    for (size_t i = 0; i < num_tasks && offset < file_size; i++) {
        size_t to_read = std::min(block_size, file_size - offset);
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_read(sqe, file, total_buffer + i * block_size, to_read,
                           offset);
        sqe->user_data = i;
        offset += to_read;
    }

    while (offset < file_size || left > 0) {
        io_uring_submit_and_wait(&ring, 1);
        io_uring_cqe *cqe;
        unsigned head;
        size_t processed = 0;
        io_uring_for_each_cqe(&ring, head, cqe) {
            size_t idx = (size_t)cqe->user_data;
            if (cqe->res < 0) {
                std::fprintf(stderr, "io_uring read error: %d\n", cqe->res);
                io_uring_queue_exit(&ring);
                munmap(data, total_buffer_size);
                close(file);
                return 1;
            }
            left -= cqe->res;
            // Launch new read if there's more data
            if (offset < file_size) {
                size_t to_read = std::min(block_size, file_size - offset);
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, file, total_buffer + idx * block_size,
                                   to_read, offset);
                sqe->user_data = idx;
                offset += to_read;
            }
            processed++;
        }
        io_uring_cq_advance(&ring, processed);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughput = static_cast<double>(file_size) / elapsed.count() /
                        (1024 * 1024); // MB/s
    std::printf(
        "time_ms:%ld\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("throughput_mbps:%.2f\n", throughput);

    io_uring_queue_exit(&ring);
    munmap(data, total_buffer_size);
    close(file);
    return 0;
}