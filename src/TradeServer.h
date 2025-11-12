// TradeServer.h
// A minimal epoll-based TCP server wrapper for TradeSim.
// It owns the listening socket, epoll instance and a periodic timer,
// accepts clients, dispatches readable events to Client instances,
// and provides a simple broadcast facility for market data.

#pragma once

#include <memory>
#include <unordered_map>

struct epoll_event;

class Client;                // forward declaration (defined in Client.h)
class MarketDataGenerator;   // forward declaration (defined in MarketDataGenerator.h)

class TradeServer {
public:
    // Construct without starting the loop. Call init() then run().
    explicit TradeServer(uint16_t port = 8000);
    ~TradeServer();

    // Create listen socket, epoll fd and timerfd.
    // Return true on success.
    bool init();

    // Blocking event loop; returns when stop() is called or fatal error.
    void run();

    // Request loop to exit gracefully.
    void stop();

    // Helper to arm EPOLLOUT when there is pending data
    void notifyWritable(int fd);
    
    // Set market data generator used on timer ticks (server takes ownership).
    void setMarketDataGenerator(std::unique_ptr<MarketDataGenerator> mdg);

private:

    // Broadcast raw bytes to all clients (thread-safe).
    void broadcast(std::string data);
    // Handlers for epoll events
    void handleAccept();
    void handleTimer();
    void handleReadable(int fd);
    void handleWritable(int fd);
    void closeClient(int fd);

    uint16_t port_;
    int epfd_{-1};
    int listen_fd_{-1};
    int timer_fd_{-1};
    bool running_{false};

    // All active clients, keyed by fd (value owns per-connection state).
    std::unordered_map<int, std::unique_ptr<Client>> clients_;

    // Market data generator for periodic broadcast.
    std::unique_ptr<MarketDataGenerator> mdg_;
};
