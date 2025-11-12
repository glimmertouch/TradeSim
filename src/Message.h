#pragma once

#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

#define MESSAGE_TYPE_LIST \
    X(Login, "login")

enum class MessageType {
#define X(type, str) type,
    MESSAGE_TYPE_LIST
#undef X
};

class Message {
    MessageType type_;
    MessageType getType() const { return type_; }
public:
    virtual ~Message() = default;

    virtual const json& handle() = 0;

    virtual void toJson() {
        #define X(msg_type, msg_str) \
        if (type_ == MessageType::msg_type) { \
            data_["action"] = msg_str; \
        }
        MESSAGE_TYPE_LIST
        #undef X
        data_["status"] = status_code_;
    }
protected:
    int status_code_;
    json data_;
    Message(MessageType type_) : type_(type_), status_code_(0) {}
};