#pragma once

#include <map>
#include <mutex>
#include <string>
#include <atomic>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <memory>
#include "ThreadSafeQueue.h"
#include <functional>

#include "OrderBook.h"

class ClientSession; // forward
class MatchingEngine {
private:
    std::map<std::string, std::unique_ptr<OrderBook>> books_;
    std::int64_t now_ms_;
    std::atomic<std::uint64_t> next_order_id_{1};
    std::atomic<std::uint64_t> next_trade_id_{1};
    struct OrderRequest {
        OrderInfo order;
        std::uint64_t order_id;
    };

    // per-product queues and worker threads
    std::unordered_map<std::string, std::unique_ptr<ThreadSafeQueue<OrderRequest>>> queues_;
    std::unordered_map<std::string, std::unique_ptr<std::thread>> workers_;
    std::atomic<bool> stopping_{false};
    // map order_id -> session pointer (registered when order submitted)
    std::unordered_map<std::uint64_t, ClientSession*> order_to_session_;
    std::mutex order_session_mtx_;
    
    void startWorkerForProduct(const std::string& product);
    void stopAllWorkers();
    
public:
    MatchingEngine();
    ~MatchingEngine();
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    // return (status code, order id). Accept session pointer so engine registers mapping atomically.
    std::pair<int, uint64_t> submitIocOrder(OrderInfo order, ClientSession* session);
    // register mapping from order id to the session that submitted it
    void registerOrderSession(std::uint64_t order_id, ClientSession* sess);
    // find session for an order id; returns nullptr if not found
    ClientSession* getSessionForOrder(std::uint64_t order_id);
    
    void checkTick(int64_t now_ms, OrderBook& book);
    std::string makeMarketData();

};
