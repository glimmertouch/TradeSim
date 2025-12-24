#include "MatchingEngine.h"

static inline int64_t getCurrentTimeInMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

MatchingEngine::MatchingEngine() {
    books_.try_emplace("A", std::make_unique<OrderBook>(100, 50));
}

OrderAck MatchingEngine::submitIocOrder(OrderInfo order) {
    OrderAck res{};
    // generate a unique order id
    if (order.quantity <= 0 || order.price <= 0 || 
        (order.side != Side::Buy && order.side != Side::Sell)) {
        res.status_code = 400;
        return res;
    }

    auto it = books_.find(order.product);
    if (it == books_.end()) {
        res.status_code = 400;
        return res;
    }
    auto& book = it->second;
    std::uint64_t order_id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
    res = book->processIocOrder(order_id, next_trade_id_, order);
    return res;
}


void MatchingEngine::checkTick(int64_t now_ms, OrderBook& book) {
    auto next_tick_ms_ = book.getNextTickTime();
    if (next_tick_ms_ == 0) {
        book.setNextTickTime(now_ms);
    }
    if (now_ms >= next_tick_ms_) {
        book.rebuildAround();
        book.setNextTickTime(now_ms);
    }
}

std::string MatchingEngine::makeMarketData() {
    now_ms_ = getCurrentTimeInMilliseconds();
    json j;
    j["action"] = "market_data";
    j["event"] = "market_data";
    // 修改：遍历时使用非 const 引用，并将 now_ms 传入 checkTick
    for (auto& [symbol, ob] : books_) {
        checkTick(now_ms_, *ob);
        j["data"][symbol] = ob->getTop5OfBook();
    }
    j["timestamp"] = now_ms_; // 使用外部传入的时间戳
    return j.dump();
}