#pragma once

#include "Message.h"

class LoginMessage : public Message {
    std::string username;
    std::string password;
public:
    LoginMessage(MessageType type_, json j);
    const json& handle() override;
    void toJson() override;
};
