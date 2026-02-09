#include "SubmitMessage.h"
#include "ClientSession.h"
#include "MatchingEngine.h"

SubmitMessage::SubmitMessage(MessageType type_, ClientSession* session, json j) 
    : Message(type_, session) {
    is_valid_ = j.contains("data") && 
                j["data"].contains("product") &&
                j["data"].contains("price") && j["data"]["price"].is_number_integer() &&
                j["data"].contains("quantity") && j["data"]["quantity"].is_number_integer() &&
                j.contains("ordertype") &&
                j.contains("side");
    if (is_valid_) {
        order_.product = j["data"]["product"].get<std::string>();
        order_.price = j["data"]["price"].get<int>();
        order_.quantity = j["data"]["quantity"].get<int>();
        order_.order_type = j["ordertype"].get<std::string>();
        std::string side = j["side"].get<std::string>();
        if (side == "buy") {
            order_.side = Side::Buy;
        } else if (side == "sell") {
            order_.side = Side::Sell;
        } else {
            is_valid_ = false;
        }
    }
}

const json& SubmitMessage::handle() {
    status_code_ = 400;

    if (!is_valid_) {
        toJson();
        return data_;
    }

    if (!session_ || session_->getUserId() < 0) {
        status_code_ = 401;
        toJson();
        return data_;
    }

    auto engine = session_->getEngine();
    if (!engine) {
        status_code_ = 500;
        toJson();
        return data_;
    }

    std::tie(status_code_, order_.order_id) = engine->submitIocOrder(std::move(order_), session_);

    toJson();
    return data_;
}
void SubmitMessage::toJson() {
    Message::toJson();
    switch (status_code_) {
        case 200:
            break;
        case 400:
            data_["error"] = "Invalid order data";
            break;
        case 401:
            data_["error"] = "Unauthorized: please log in";
            break;
        case 500:
            data_["error"] = "Internal server error";
            break;
        default:
            data_["error"] = "Unknown status";
            break;
    }
    data_["response"] = order_.tojson(status_code_ == 200);
}
