#pragma once

#include <nlohmann/json.hpp>
#include "OrderBook.h"

using nlohmann::json;

class TradeMessage {
    json data_;
public:
    explicit TradeMessage(const TradeExecution& exec) {
        data_["action"] = "trade";
        data_["event"] = "trade";
        data_["trade"] = {
            {"trade_id", exec.trade_id},
            {"buy_order_id", exec.buy_order_id},
            {"sell_order_id", exec.sell_order_id},
            {"product", exec.product},
            {"price", exec.price},
            {"quantity", exec.quantity},
            {"timestamp", exec.timestamp}
        };
        // 可在此处添加额外字段，例如来源、序列号等
        // data_["source"] = "matching_engine";
    }

    const json& toJson() const {
        return data_;
    }
};
