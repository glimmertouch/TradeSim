#pragma once

#include "Message.h"
#include "OrderBook.h"

class SubmitMessage : public Message {
    OrderInfo order_;
public:
    SubmitMessage(MessageType type_, ClientSession* session, json j);
    const json& handle() override;
    void toJson() override;
};
