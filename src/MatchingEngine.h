#pragma once

#include <map>
#include <mutex>
#include <string>
#include <atomic>
#include <cstdint>

#include "OrderBook.h"

class MatchingEngine {
public:
    MatchingEngine();

    // return status code
    OrderAck submitIocOrder(OrderInfo order);

    void checkTick(int64_t now_ms, OrderBook& book);
    std::string makeMarketData();

private:
    std::map<std::string, std::unique_ptr<OrderBook>> books_;
    std::int64_t now_ms_;
    std::atomic<std::uint64_t> next_order_id_{1};
    std::atomic<std::uint64_t> next_trade_id_{1};
};
