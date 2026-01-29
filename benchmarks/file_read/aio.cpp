#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <libaio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

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

    io_context_t ctx = 0;
    if (io_setup(num_tasks, &ctx) < 0) {
        perror("io_setup");
        munmap(data, total_buffer_size);
        close(file);
        return 1;
    }

    size_t offset = 0;
    size_t completed = 0;
    std::vector<iocb> cbs(num_tasks);
    std::vector<iocb *> cbs_ptr(num_tasks);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_tasks; ++i) {
        cbs_ptr[i] = &cbs[i];
    }

    std::vector<bool> task_active(num_tasks, false);

    while (offset < file_size || std::any_of(
                                     task_active.begin(), task_active.end(),
                                     [](bool v) { return v; })) {
        for (size_t i = 0; i < num_tasks; ++i) {
            if (!task_active[i] && offset < file_size) {
                size_t to_read = std::min(block_size, file_size - offset);
                memset(&cbs[i], 0, sizeof(iocb));
                io_prep_pread(&cbs[i], file, total_buffer + i * block_size,
                              to_read, offset);
                cbs[i].data = (void *)i;
                if (io_submit(ctx, 1, &cbs_ptr[i]) < 0) {
                    perror("io_submit");
                    io_destroy(ctx);
                    munmap(data, total_buffer_size);
                    close(file);
                    return 1;
                }
                task_active[i] = true;
                offset += to_read;
            }
        }

        io_event events[num_tasks];
        int ret = io_getevents(ctx, 1, num_tasks, events, nullptr);
        if (ret < 0) {
            perror("io_getevents");
            io_destroy(ctx);
            munmap(data, total_buffer_size);
            close(file);
            return 1;
        }
        for (int j = 0; j < ret; ++j) {
            size_t idx = (size_t)events[j].obj->data;
            if (events[j].res < 0) {
                std::fprintf(stderr, "AIO read error: %zd\n", events[j].res);
                io_destroy(ctx);
                munmap(data, total_buffer_size);
                close(file);
                return 1;
            }
            task_active[idx] = false;
            completed++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double throughput = static_cast<double>(file_size) / elapsed.count() /
                        (1024 * 1024); // MB/s
    std::printf(
        "time_ms:%ld\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    std::printf("throughput_mbps:%.2f\n", throughput);

    io_destroy(ctx);
    munmap(data, total_buffer_size);
    close(file);
    return 0;
}