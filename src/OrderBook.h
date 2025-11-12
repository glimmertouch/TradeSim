#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <random>
using nlohmann::json;

struct buildParams {
    double d;  // depth decay
    double t;  // tilt
    double round_mult; // rounding volume multiplier
    int max_step; // mid price step
    int max_tick; // max tick size
    double gap_prob; // probability of missing a level
};

class OrderBook {
    std::map<int, int, std::greater<>> bids; // buy orders
    std::map<int, int> asks;                 // sell orders
    std::mt19937 gen{std::random_device{}()};
    std::int64_t next_tick_ms_{0}; // 下次触发 tick 的时间戳（毫秒，unix time）
    int min_interval_ms_ = 2000;
    int max_interval_ms_ = 5000;
    int mid_price_;
    int max_volume_;

    buildParams params_{0.15, 0.2, 2.0, 5, 5, 0.33};

public:
    OrderBook();
    OrderBook(int fair_price, int max_volume);
    OrderBook(int fair_price, int max_volume, const buildParams& params);

    json getTop5OfBook() const;
    void rebuildAround();

    
    std::int64_t getNextTickTime();
    void setNextTickTime(std::int64_t now_ms_);

    int getMidPrice() const;

};