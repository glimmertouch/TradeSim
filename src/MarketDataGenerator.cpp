#include "MarketDataGenerator.h"

static inline int64_t getCurrentTimeInMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

MarketDataGenerator::MarketDataGenerator() {
    order_books["A"] = OrderBook(100, 50);
}


void MarketDataGenerator::checkTick(int64_t now_ms, OrderBook& book) {
    auto next_tick_ms_ = book.getNextTickTime();
    if (next_tick_ms_ == 0) {
        book.setNextTickTime(now_ms);
    }
    if (now_ms >= next_tick_ms_) {
        book.rebuildAround();
        book.setNextTickTime(now_ms);
    }
}


std::string MarketDataGenerator::makeMarketData() {
    now_ms_ = getCurrentTimeInMilliseconds();
    json j;
    j["action"] = "market_data";
    j["event"] = "market_data";
    // 修改：遍历时使用非 const 引用，并将 now_ms 传入 checkTick
    for (auto& [symbol, ob] : order_books) {
        checkTick(now_ms_, ob);
        j["data"][symbol] = ob.getTop5OfBook();
    }
    j["timestamp"] = now_ms_; // 使用外部传入的时间戳
    return j.dump();
}

