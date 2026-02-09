#include "MatchingEngine.h"
#include "ClientSession.h"
#include "TradeMessage.h"

static inline int64_t getCurrentTimeInMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

MatchingEngine::MatchingEngine() {
    books_.try_emplace("A", std::make_unique<OrderBook>(100, 50));
    // create queue and worker for product A
    startWorkerForProduct("A");
}

MatchingEngine::~MatchingEngine() {
    stopAllWorkers();
}

std::pair<int, uint64_t> MatchingEngine::submitIocOrder(OrderInfo order, ClientSession* session) {
    if (order.quantity <= 0 || order.price <= 0 ||
        (order.side != Side::Buy && order.side != Side::Sell)) {
        return {400, 0};
    }

    auto it = books_.find(order.product);
    if (it == books_.end()) {
        return {400, 0};
    }

    // generate order id, register mapping and enqueue request, then return immediately
    std::uint64_t order_id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
    auto qit = queues_.find(order.product);
    if (qit == queues_.end()) {
        // no queue for this product; return error
        return {500, 0};
    }

    // register the order->session mapping before pushing to queue to avoid races
    registerOrderSession(order_id, session);

    OrderRequest req{order, order_id};
    qit->second->push(std::move(req));

    return {200, order_id};
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

void MatchingEngine::startWorkerForProduct(const std::string& product) {
    if (queues_.count(product) || workers_.count(product)) return;
    queues_.emplace(product, std::make_unique<ThreadSafeQueue<OrderRequest>>());
    auto queue_ptr = queues_[product].get();
    // start worker thread
    workers_.emplace(product, std::make_unique<std::thread>([this, product, queue_ptr]() {
        OrderRequest req;
        while (!stopping_) {
            if (!queue_ptr->wait_and_pop(req)) break; // queue closed
            // find book
            auto it = books_.find(product);
            if (it == books_.end()) {
                continue;
            }
            auto& book = it->second;
            // process the IOC under book's mutex inside processIocOrder
            auto executions = book->processIocOrder(req.order_id, next_trade_id_, req.order);
            for (auto &ex : executions) {
                if (ex.buy_order_id != 0) {
                    ClientSession* buyer = getSessionForOrder(ex.buy_order_id);
                    if (buyer) {
                        TradeMessage tm(ex);
                        buyer->appendToWriteBuffer(tm.toJson().dump());
                    }
                }
                if (ex.sell_order_id != 0) {
                    ClientSession* seller = getSessionForOrder(ex.sell_order_id);
                    if (seller) {
                        TradeMessage tm(ex);
                        seller->appendToWriteBuffer(tm.toJson().dump());
                    }
                }
            }
            
            // If this incoming request was an IOC order, it's completed after processing;
            // safe to remove order->session mapping immediately to avoid holding stale entries.
            auto ot = req.order.order_type;
            if (ot == "ioc") {
                std::lock_guard<std::mutex> lk(order_session_mtx_);
                order_to_session_.erase(req.order_id);
            }
            
        }
    }));
}

void MatchingEngine::stopAllWorkers() {
    stopping_ = true;
    for (auto &p : queues_) p.second->close();
    for (auto &w : workers_) {
        if (w.second && w.second->joinable()) w.second->join();
    }
    workers_.clear();
    queues_.clear();
}

void MatchingEngine::registerOrderSession(std::uint64_t order_id, ClientSession* sess) {
    if (order_id == 0 || sess == nullptr) return;
    std::lock_guard<std::mutex> lk(order_session_mtx_);
    order_to_session_[order_id] = sess;
}

ClientSession* MatchingEngine::getSessionForOrder(std::uint64_t order_id) {
    if (order_id == 0) return nullptr;
    std::lock_guard<std::mutex> lk(order_session_mtx_);
    auto it = order_to_session_.find(order_id);
    if (it == order_to_session_.end()) return nullptr;
    return it->second;
}