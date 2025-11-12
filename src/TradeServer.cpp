#include "TradeServer.h"

#include "Client.h"
#include "MarketDataGenerator.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <iostream>


TradeServer::TradeServer(uint16_t port) : port_(port) {}

TradeServer::~TradeServer() { stop(); }

void TradeServer::setMarketDataGenerator(std::unique_ptr<MarketDataGenerator> mdg) {
    mdg_ = std::move(mdg);
}

bool TradeServer::init() {
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ < 0) {
        std::perror("epoll_create1");
        return false;
    }

    // 1) listen socket (non-blocking)
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        std::perror("socket");
        return false;
    }
    int yes = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return false;
    }
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        std::perror("listen");
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
        std::perror("epoll_ctl listen");
        return false;
    }

    // 2) timerfd for periodic tasks (250ms)
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0) {
        std::perror("timerfd_create");
        return false;
    }
    itimerspec its{};
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 250000000; // 250 ms
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 250000000;
    if (timerfd_settime(timer_fd_, 0, &its, nullptr) < 0) {
        std::perror("timerfd_settime");
        return false;
    }
    epoll_event tev{};
    tev.events = EPOLLIN;
    tev.data.fd = timer_fd_;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, timer_fd_, &tev) < 0) {
        std::perror("epoll_ctl timer");
        return false;
    }

    std::cout << "listening on 127.0.0.1:" << port_ << "\n";
    return true;
}

void TradeServer::run() {
    running_ = true;
    epoll_event events[1024];
    while (running_) {
        int n = epoll_wait(epfd_, events, 1024, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t e = events[i].events;

            if (fd == listen_fd_ && (e & EPOLLIN)) {
                handleAccept();
            } else if (fd == timer_fd_ && (e & EPOLLIN)) {
                handleTimer();
            } else {
                if (e & (EPOLLHUP | EPOLLERR)) {
                    closeClient(fd);
                    continue;
                }
                if (e & EPOLLIN) {
                    handleReadable(fd);
                }
                if (e & EPOLLOUT) handleWritable(fd);
            }
        }
    }
}

void TradeServer::stop() {
    if (!running_) return;
    running_ = false;
    // Close clients
    for (auto it = clients_.begin(); it != clients_.end(); ) {
        int fd = it->first;
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        it = clients_.erase(it);
    }
    if (timer_fd_ >= 0) { epoll_ctl(epfd_, EPOLL_CTL_DEL, timer_fd_, nullptr); close(timer_fd_); timer_fd_ = -1; }
    if (listen_fd_ >= 0) { epoll_ctl(epfd_, EPOLL_CTL_DEL, listen_fd_, nullptr); close(listen_fd_); listen_fd_ = -1; }
    if (epfd_ >= 0) { close(epfd_); epfd_ = -1; }
}

void TradeServer::broadcast(std::string data) {
    for (auto& [fd, c] : clients_) {
        c->appendToWriteBuffer(data);
        notifyWritable(fd);
    }
}

void TradeServer::handleAccept() {
    for (;;) {
        int cfd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::perror("accept");
            break;
        }
        auto cli = std::make_unique<Client>(cfd);
        // When client has new data to send, arm EPOLLOUT on its fd
        cli->setWritableNotifier([this](int fd){ this->notifyWritable(fd); });
        epoll_event ce{};
        ce.events = EPOLLIN; // start with read interest only
        ce.data.fd = cfd;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &ce) < 0) {
            std::perror("epoll_ctl add client");
            close(cfd);
            continue;
        }
        clients_.emplace(cfd, std::move(cli));
    }
}

void TradeServer::handleTimer() {
    // drain timer ticks
    std::uint64_t ticks;
    while (read(timer_fd_, &ticks, sizeof(ticks)) > 0) {}

    if (mdg_) {
        std::string payload = mdg_->makeMarketData();
        broadcast(std::move(payload));
    }
}

void TradeServer::handleReadable(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    Client* client = it->second.get();
    char buf[4096];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r > 0) {
            client->appendToReadBuffer(std::string(buf, r));
        } else if (r == 0) {
            closeClient(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            closeClient(fd);
            return;
        }
    }
    while (client->tryParseReadBuffer()) {
        // messages will be queued and processed by client's worker thread
    }
    // If client enqueued response meanwhile, ensure writable is armed
    // We check at write time; alternatively add hasPendingWrite() and arm here.
}

void TradeServer::handleWritable(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    Client* client = it->second.get();
    // Ask client to flush its buffered data
    ssize_t sent = client->flushWriteBufferNonBlocking();
    (void)sent;
    // If no more pending data, remove EPOLLOUT interest
    // Keep EPOLLIN to continue reading.
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    // Only clear EPOLLOUT when buffer is empty
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        std::perror("epoll_ctl mod clear EPOLLOUT");
    }
}

void TradeServer::closeClient(int fd) {
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    clients_.erase(fd);
}

void TradeServer::notifyWritable(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = fd;
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        std::perror("epoll_ctl mod writable");
    }
}

