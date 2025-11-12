#pragma once

#include <map>
#include <string>

#include "OrderBook.h"

using nlohmann::json;

class MarketDataGenerator {
    std::map<std::string, OrderBook> order_books;
    std::int64_t now_ms_;
public:
    MarketDataGenerator();
    void checkTick(int64_t now_ms, OrderBook& book);
    std::string makeMarketData();
};

