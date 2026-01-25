#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr size_t BACKLOG = 128;
constexpr size_t MAX_CONNECTIONS = 1024;
constexpr size_t MAX_MESSAGE_LEN = 2048;

void prepare_address(const std::string &host, uint16_t port,
                     sockaddr_in &addr) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    std::string host = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    sockaddr_in server_addr;
    prepare_address(host, port, server_addr);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::perror("Failed to create socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    if (set_nonblocking(server_fd) < 0) {
        std::perror("Failed to set server socket non-blocking");
        close(server_fd);
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::perror("Failed to create epoll");
        close(server_fd);
        return 1;
    }

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        std::perror("Failed to add server fd to epoll");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    std::printf("Echo server listening on %s:%d\n", host.c_str(), port);

    epoll_event events[MAX_CONNECTIONS];
    char buffer[MAX_MESSAGE_LEN];

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_CONNECTIONS, -1);
        if (nfds < 0) {
            if (errno == EINTR)
                continue;
            std::perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_fd) {
                // New incoming connection
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(
                    server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    std::perror("Failed to accept connection");
                    continue;
                }
                if (set_nonblocking(client_fd) < 0) {
                    std::perror("Failed to set client socket non-blocking");
                    close(client_fd);
                    continue;
                }
                epoll_event client_ev;
                client_ev.events = EPOLLIN | EPOLLET;
                client_ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) <
                    0) {
                    std::perror("Failed to add client fd to epoll");
                    close(client_fd);
                    continue;
                }
            } else {
                // Connected client has data to read
                while (true) {
                    ssize_t n = recv(fd, buffer, MAX_MESSAGE_LEN, 0);
                    if (n > 0) {
                        ssize_t sent = 0;
                        while (sent < n) {
                            ssize_t m = send(fd, buffer + sent, n - sent, 0);
                            if (m > 0) {
                                sent += m;
                            } else if (m < 0 && (errno == EAGAIN ||
                                                 errno == EWOULDBLOCK)) {
                                // Send buffer full, try again later
                                break;
                            } else {
                                // Send error
                                close(fd);
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                                break;
                            }
                        }
                    } else if (n == 0) {
                        // Client closed connection
                        close(fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        break;
                    } else if (n < 0 &&
                               (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        // Finished reading
                        break;
                    } else {
                        // Other errors
                        close(fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        break;
                    }
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}