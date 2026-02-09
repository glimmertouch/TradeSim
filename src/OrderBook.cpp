#include "OrderBook.h"

OrderBook::OrderBook() = default;

OrderBook::OrderBook(int fair_price, int max_volume) : mid_price_(fair_price), max_volume_(max_volume) {
    rebuildAround();
}

OrderBook::OrderBook(int fair_price, int max_volume, const build_params& params) 
            : mid_price_(fair_price), max_volume_(max_volume), params_(params) {
    rebuildAround();
}

json OrderBook::getTop5OfBook() const {
    json j;
    if (!bids.empty()) {
        for (auto it = bids.begin(); it != bids.end() && j["buy"].size() < 5; ++it) {
            j["buy"].push_back({{"price", it->first}, {"volume", it->second}});
        }
    } else {
        j["buy"] = nullptr;
    }

    if (!asks.empty()) {
        for (auto it = asks.begin(); it != asks.end() && j["sell"].size() < 5; ++it) {
            j["sell"].push_back({{"price", it->first}, {"volume", it->second}});
        }
    } else {
        j["sell"] = nullptr;
    }
    return j;
}

void OrderBook::rebuildAround() {
    asks.clear();
    bids.clear();
    auto& [d, t, round_mult, max_step, max_tick, gap_prob] = params_;
    
    std::uniform_int_distribution<int> step_dist(-max_step, max_step);
    mid_price_ = std::max(1, mid_price_ + step_dist(gen));
    
    double lambda0 = 0.75 * max_volume_;
    std::uniform_real_distribution<> tilt_dist(-t, t);
    std::uniform_int_distribution<> tick_dist(1, max_tick);
    std::bernoulli_distribution gap(gap_prob);

    auto priceLevelVolume = [&](int price_level, int price, int side) {
        double prob = std::exp(-d * price_level);
        double expected_volume = lambda0 * prob;
        double volume = expected_volume * (side == 1 ? (1 + tilt_dist(gen)) : (1 - tilt_dist(gen)));
        
        if (price % 5 == 0) {
            volume *= round_mult;
        }
        std::poisson_distribution<int> volume_dist(volume);
        return std::max(1, volume_dist(gen));
    };

    int tick = tick_dist(gen), buy1 = mid_price_, sell1 = mid_price_ + tick;
    for (int i = 0; i < 20; ++i) {
        if (!gap(gen)) {
            bids[buy1 - i] = priceLevelVolume(i, buy1 - i, 1);
        }
        if (!gap(gen)) {
            asks[sell1 + i] = priceLevelVolume(i, sell1 + i, 0);
        }
    }
}

std::vector<TradeExecution> OrderBook::processIocOrder(std::uint64_t order_id, std::atomic<std::uint64_t>& trade_id,
                                                     const OrderInfo& order) {
    std::vector<TradeExecution> executions;

    int price = order.price;
    int quantity = order.quantity;
    Side side = order.side;
    std::string product = order.product;

    std::lock_guard<std::mutex> lock(mtx_);
    if (side == Side::Buy) {
        // Process buy IOC order
        auto it = asks.begin();
        while (it != asks.end() && quantity > 0 && it->first <= price) {
            int trade_qty = std::min(quantity, it->second);
            quantity -= trade_qty;
            it->second -= trade_qty;

            TradeExecution te{
                .trade_id = trade_id.fetch_add(1, std::memory_order_relaxed),
                .buy_order_id = order_id,
                .sell_order_id = 0,
                .product = product,
                .price = it->first,
                .quantity = trade_qty,
                .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()
            };
            executions.emplace_back(std::move(te));

            if (it->second == 0) {
                it = asks.erase(it);
            } else {
                ++it;
            }
        }
    } else if (side == Side::Sell) {
        // Process sell IOC order
        auto it = bids.begin();
        while (it != bids.end() && quantity > 0 && it->first >= price) {
            int trade_qty = std::min(quantity, it->second);
            quantity -= trade_qty;
            it->second -= trade_qty;

            TradeExecution te{
                .trade_id = trade_id.fetch_add(1, std::memory_order_relaxed),
                .buy_order_id = 0,
                .sell_order_id = order_id,
                .product = product,
                .price = it->first,
                .quantity = trade_qty,
                .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()
            };
            executions.emplace_back(std::move(te));

            if (it->second == 0) {
                it = bids.erase(it);
            } else {
                ++it;
            }
        }
    }

    return executions;
}

std::int64_t OrderBook::getNextTickTime() {
    return next_tick_ms_;
}

int OrderBook::getMidPrice() const {
    return mid_price_;
}

void OrderBook::setNextTickTime(std::int64_t now_ms_) {
    std::uniform_int_distribution<int> dist(min_interval_ms_, max_interval_ms_);
    next_tick_ms_ = now_ms_ + dist(gen);
}
