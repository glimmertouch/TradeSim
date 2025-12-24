#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <random>
#include <atomic>

using nlohmann::json;

enum class Side {
    Buy,
    Sell
};

struct OrderInfo {
    std::string product;
    int price;
    int quantity;
    Side side;
};

struct TradeExecution {
    std::uint64_t trade_id;
    std::uint64_t buy_order_id;
    std::uint64_t sell_order_id;
    std::string product;
    int price;
    int quantity;
    std::int64_t timestamp; // unix time in milliseconds
};

struct OrderAck {
    int status_code;
    std::uint64_t order_id;
    std::vector<TradeExecution> executions;
};

class OrderBook {
    struct build_params {
        double d;  // depth decay
        double t;  // tilt
        double round_mult; // rounding volume multiplier
        int max_step; // mid price step
        int max_tick; // max tick size
        double gap_prob; // probability of missing a level
    };
    std::map<int, int, std::greater<>> bids; // buy orders
    std::map<int, int> asks;                 // sell orders
    std::mt19937 gen{std::random_device{}()};
    std::int64_t next_tick_ms_{0}; 
    int min_interval_ms_ = 2000;
    int max_interval_ms_ = 5000;
    int mid_price_;
    int max_volume_;
    std::mutex mtx_;

    build_params params_{0.15, 0.2, 2.0, 5, 5, 0.33};

public:
    OrderBook();
    OrderBook(int fair_price, int max_volume);
    OrderBook(int fair_price, int max_volume, const build_params& params);

    json getTop5OfBook() const;
    void rebuildAround();

    OrderAck processIocOrder(std::uint64_t order_id, std::atomic<std::uint64_t>& trade_id, const OrderInfo& order);
    
    std::int64_t getNextTickTime();
    void setNextTickTime(std::int64_t now_ms_);

    int getMidPrice() const;

};