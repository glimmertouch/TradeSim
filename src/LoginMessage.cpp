#include "LoginMessage.h"


LoginMessage::LoginMessage(MessageType type_, json j) : Message(type_) {
    if (j.contains("username")) {
        username = j["username"].get<std::string>();
    }
    if (j.contains("password")) {
        password = j["password"].get<std::string>();
    }
}

const json& LoginMessage::handle() {
    if (username.empty() || password.empty()) {
        status_code_ = 403;
    } else {
        // For demonstration, accept any non-empty username/password
        status_code_ = 200;
    }
    toJson();
    return data_;
}

void LoginMessage::toJson() {
    Message::toJson();
    if (status_code_ == 200) {
        data_["msg"] = "Login successful";
    } else {
        data_["error"] = "Invalid username or password";
    }
}
