#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

static std::string address = "127.0.0.1";
static int port = 12345;
static int message_length = 512;
static int connection_count = 50;
static int test_duration_s = 60;

struct Count {
    uint64_t inbytes = 0;
    uint64_t outbytes = 0;
};

void usage(const char *prog_name) {
    std::printf(
        "Usage: %s [-h] [-a address] [-p port] [-l length] [-c number] [-t "
        "duration]\n"
        "  -h           Show this help message\n"
        "  -a address   Specify the server address\n"
        "  -p port      Specify the server port\n"
        "  -l length    Specify the message length\n"
        "  -c number    Specify the number of connections\n"
        "  -t duration  Specify the test duration in seconds\n",
        prog_name);
}

int do_connect(const std::string &address, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void do_echo(std::atomic<bool> &running, Count &count) {
    std::string message(message_length, 'x');
    std::vector<char> buffer(message_length);

    int sockfd = do_connect(address, port);
    if (sockfd < 0) {
        std::perror("Connection failed");
        exit(1);
    }

    while (true) {
        if (!running.load()) [[unlikely]] {
            break;
        }

        int r = 0;
        while (r < message_length) {
            int n = send(sockfd, message.data() + r, message_length - r, 0);
            if (n <= 0) [[unlikely]] {
                std::perror("Send failed");
                close(sockfd);
                exit(1);
            }
            r += n;
            count.outbytes += n;
        }

        if (!running.load()) [[unlikely]] {
            break;
        }

        r = recv(sockfd, buffer.data(), message_length, 0);
        if (r <= 0) [[unlikely]] {
            std::perror("Receive failed");
            close(sockfd);
            exit(1);
        }
        count.inbytes += r;
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "ha:p:l:c:t:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'a':
            address = optarg;
            break;
        case 'p':
            port = std::atoi(optarg);
            break;
        case 'l':
            message_length = std::atoi(optarg);
            break;
        case 'c':
            connection_count = std::atoi(optarg);
            break;
        case 't':
            test_duration_s = std::atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    std::vector<Count> counts(connection_count);

    std::atomic<bool> running(true);
    std::vector<std::thread> threads;
    for (int i = 0; i < connection_count; ++i) {
        threads.emplace_back(do_echo, std::ref(running), std::ref(counts[i]));
    }
    sleep(test_duration_s);
    running.store(false);
    for (auto &t : threads) {
        t.join();
    }

    uint64_t total_inbytes = 0;
    uint64_t total_outbytes = 0;
    for (const auto &c : counts) {
        total_inbytes += c.inbytes;
        total_outbytes += c.outbytes;
    }

    float req_bytes_per_sec =
        static_cast<float>(total_outbytes) / test_duration_s;
    float resp_bytes_per_sec =
        static_cast<float>(total_inbytes) / test_duration_s;
    std::printf("req_bytes_per_sec:%.2f\nresp_bytes_per_sec:%.2f\n",
                req_bytes_per_sec, resp_bytes_per_sec);
}