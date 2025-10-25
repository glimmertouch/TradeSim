#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <unordered_set>

constexpr uint16_t PORT = 8000;
constexpr char MSG[] = "hello, user\n";

int main() {
    // 1) listen socket (non-blocking)
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sfd < 0) {
        std::perror("socket");
        return 1;
    }
    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    if (bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return 1;
    }
    if (listen(sfd, SOMAXCONN) < 0) {
        std::perror("listen");
        return 1;
    }

    // 2) epoll + 1s periodic timer
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        std::perror("epoll_create1");
        return 1;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) < 0) {
        std::perror("epoll_ctl listen");
        return 1;
    }

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        std::perror("timerfd_create");
        return 1;
    }
    itimerspec its{};
    its.it_interval.tv_sec = 1;
    its.it_value.tv_sec = 1;  // every 1s
    if (timerfd_settime(tfd, 0, &its, nullptr) < 0) {
        std::perror("timerfd_settime");
        return 1;
    }
    epoll_event tev{};
    tev.events = EPOLLIN;
    tev.data.fd = tfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &tev) < 0) {
        std::perror("epoll_ctl timer");
        return 1;
    }

    std::unordered_set<int> clients;
    std::cout << "listening on 127.0.0.1:" << PORT << "\n";

    epoll_event events[1024];
    for (;;) {
        int n = epoll_wait(epfd, events, 1024, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t e = events[i].events;

            if (fd == sfd && (e & EPOLLIN)) {
                // accept all pending
                for (;;) {
                    int c = accept4(sfd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (c < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        std::perror("accept");
                        break;
                    }
                    clients.insert(c);
                    epoll_event ce{};
                    ce.events = EPOLLIN;
                    ce.data.fd = c;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, c, &ce);
                }
            } else if (fd == tfd && (e & EPOLLIN)) {
                // consume timer tick(s)
                std::uint64_t ticks;
                while (read(tfd, &ticks, sizeof(ticks)) > 0) {
                }
                // broadcast one line to all clients
                for (auto it = clients.begin(); it != clients.end();) {
                    int cfd = *it;
                    ssize_t r = send(cfd, MSG, sizeof(MSG) - 1, MSG_NOSIGNAL);
                    if (r <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, nullptr);
                        close(cfd);
                        it = clients.erase(it);
                    } else {
                        ++it;
                    }
                }
            } else if (e & EPOLLIN) {
                // ignore readable data; we only push data periodically now
            } else {
                printf("unexpected epoll event %u on fd %d\n", e, fd);
            }
        }
    }

    // cleanup (normally unreachable)
    for (int c : clients) {
        close(c);
    }
    close(tfd);
    close(sfd);
    close(epfd);
    return 0;
}
