#include <arpa/inet.h>
#include <condy.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr size_t BACKLOG = 128;
constexpr size_t MAX_CONNECTIONS = 1024;
constexpr size_t MAX_MESSAGE_LEN = 2048;

static std::string host;
static uint16_t port;
static bool use_fixed_fd = false;

condy::Coro<void> session(int client_fd) {
    char buffer[MAX_MESSAGE_LEN];

    while (true) {
        int n;
        if (use_fixed_fd) {
            n = co_await condy::async_recv(condy::fixed(client_fd),
                                           condy::buffer(buffer), 0);
        } else {
            n = co_await condy::async_recv(client_fd, condy::buffer(buffer), 0);
        }

        if (n == 0) {
            // Connection closed by client
            break;
        }

        if (use_fixed_fd) {
            co_await condy::async_send(condy::fixed(client_fd),
                                       condy::buffer(buffer, n), 0);
        } else {
            co_await condy::async_send(client_fd, condy::buffer(buffer, n), 0);
        }
    }

    co_await condy::async_close(condy::fixed(client_fd));
}

condy::Coro<int> co_main(int server_fd) {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd;
        if (use_fixed_fd) {
            client_fd = co_await condy::async_accept_direct(
                server_fd, (struct sockaddr *)&client_addr, &client_len, 0,
                CONDY_FILE_INDEX_ALLOC);
        } else {
            client_fd = co_await condy::async_accept(
                server_fd, (struct sockaddr *)&client_addr, &client_len, 0);
        }
        if (client_fd < 0) [[unlikely]] {
            std::fprintf(stderr, "Failed to accept connection: %d\n",
                         client_fd);
            co_return 1;
        }

        condy::co_spawn(session(client_fd)).detach();
    }
}

void prepare_address(const std::string &host, uint16_t port,
                     sockaddr_in &addr) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
}

void usage(const char *prog_name) {
    std::fprintf(stderr,
                 "Usage: %s [-hf] <host> <port>\n"
                 "  -h         Show this help message\n"
                 "  -f         Use fixed file descriptor\n",
                 prog_name);
}

int main(int argc, char **argv) noexcept(false) {
    int opt;
    while ((opt = getopt(argc, argv, "hf")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'f':
            use_fixed_fd = true;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (argc - optind != 2) {
        usage(argv[0]);
        return 1;
    }

    host = argv[optind];
    port = static_cast<uint16_t>(std::stoi(argv[optind + 1]));

    sockaddr_in server_addr;
    prepare_address(host, port, server_addr);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::perror("Failed to create socket");
        return 1;
    }

    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) < 0) {
        std::perror("Failed to set socket options");
        close(server_fd);
        return 1;
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        std::perror("Failed to bind socket");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        std::perror("Failed to listen on socket");
        close(server_fd);
        return 1;
    }

    std::printf("Echo server listening on %s:%d\n", host.c_str(), port);

    condy::Runtime runtime(condy::RuntimeOptions().sq_size(MAX_CONNECTIONS));

    if (use_fixed_fd) {
        runtime.fd_table().init(MAX_CONNECTIONS);
    }

    return condy::sync_wait(runtime, co_main(server_fd));
}