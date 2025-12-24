#include "LoginMessage.h"
#include "Database.h"
#include "ClientSession.h"

LoginMessage::LoginMessage(MessageType type_, ClientSession* session, json j) 
    : Message(type_, session) {
    is_valid_ = j.contains("username") && j.contains("password");
    if (is_valid_) {
        username = j["username"].get<std::string>();
        password = j["password"].get<std::string>();
    }
}

const json& LoginMessage::handle() {
    auto& db = Database::instance();

    status_code_ = 403;
    if (is_valid_) {
        int user_id = db.verifyUser(username, password).value_or(-1);
        if (user_id != -1) {
            status_code_ = 200;
            session_->setUserId(user_id);
        }
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
