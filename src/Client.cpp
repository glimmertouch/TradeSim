#include "Client.h"
#include "MessageFactory.h"

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cctype>

using nlohmann::json;

Client::Client(int fd_) : fd(fd_), read_buffer_(), write_buffer_() {
    thread_ = std::thread(&Client::workProcess, this);
}

Client::~Client() {
    message_queue_.close();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Client::appendToReadBuffer(std::string data) {
    read_buffer_ += data;
}

void Client::appendToWriteBuffer(std::string data) {
    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_buffer_.empty()) notify = true; // transition empty -> non-empty
        write_buffer_ += std::move(data);
    }
    if (notify && writable_notifier_) {
        writable_notifier_(fd);
    }
}

void Client::workProcess() {
    std::unique_ptr<Message> msg;
    while (1) {
        if (!message_queue_.wait_and_pop(msg)) {
            break;
        }
        if (msg) {
            auto j = msg->handle();
            if (!j.is_discarded()) {
                appendToWriteBuffer(j.dump());
            }
        }
    }
} 

bool Client::tryParseReadBuffer() {
    if (read_buffer_.empty()) return false;
    try {
        size_t i = 0;
        while (i < read_buffer_.size() && std::isspace(static_cast<unsigned char>(read_buffer_[i]))) i++;
        json out = json::parse(read_buffer_.substr(i), nullptr, false);
        if (out.is_discarded()) return false;

        std::string dumped = out.dump();
        size_t consumed = read_buffer_.find(dumped, i);
        if (consumed == std::string::npos) {
            // Could not locate serialized substring exactly; clear to avoid infinite loop
            read_buffer_.clear();
        } else {
            read_buffer_.erase(0, consumed + dumped.size());
        }
        auto msg = createMessageFromJson(out);
        if (msg) {
            // Successfully created message
            message_queue_.push(std::move(msg));
        }
        return true;
    } catch (const json::parse_error&) {
        return false; // incomplete
    }
}

std::unique_ptr<Message> Client::createMessageFromJson(json j) {
    if (!j.contains("action")) return nullptr;

    std::string action = j["action"];
    std::unique_ptr<Message> msg = nullptr;


#define X(msg_type, msg_str) \
if (action == msg_str) { \
msg = std::make_unique<msg_type##Message>(MessageType::msg_type, std::move(j)); \
}

MESSAGE_TYPE_LIST

#undef X 

    return msg;
}

ssize_t Client::flushWriteBufferNonBlocking() {
    std::lock_guard<std::mutex> lock(write_mutex_);
    ssize_t total = 0;
    while (!write_buffer_.empty()) {
        ssize_t n = send(fd, write_buffer_.data(), write_buffer_.size(), MSG_NOSIGNAL);
        if (n > 0) {
            write_buffer_.erase(0, static_cast<size_t>(n));
            total += n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break; // try later when writable
        }
        // error or connection closed; let server decide to close on write error
        break;
    }
    return total;
}
