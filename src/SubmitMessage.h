#pragma once

#include "Message.h"
#include "OrderBook.h"

class SubmitMessage : public Message {
    OrderInfo order_;
    std::string order_type_;
    std::string side_;
    std::vector<TradeExecution> executions_;
public:
    SubmitMessage(MessageType type_, ClientSession* session, json j);
    const json& handle() override;
    void toJson() override;
};
