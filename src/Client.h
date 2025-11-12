#pragma once
#include <string>
#include <functional>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include "Message.h"
#include "ThreadSafeQueue.h"

/*
        epoll 主线程                        worker 线程
----------------------------------      --------------------------
recv -> appendToReadBuffer()    
     -> tryParseReadBuffer()    
        -> message_queue_.push(msg) 
                                        wait_and_pop(msg)
                                        msg->handle()
                                        client->appendWriteBuffer()
                                        server->notifyWritable()
*/
class Client {
    int fd;                     // file descriptor for connection
    std::string read_buffer_;   // accumulated inbound data
    std::string write_buffer_;  // pending outbound data
    std::mutex write_mutex_; 
    ThreadSafeQueue<std::unique_ptr<Message>> message_queue_;  // queue of messages to handle
    std::thread thread_;
    // Notify when write buffer transitions from empty to non-empty.
    std::function<void(int)> writable_notifier_{};
public:
    explicit Client(int fd_);
    ~Client();

    void appendToReadBuffer(std::string data);
    void appendToWriteBuffer(std::string data);

    void workProcess();

    // Attempts to parse a JSON object from the read buffer.
    // Returns true if a complete JSON object was found and consumed.
    // Returns false if buffer is empty or contains incomplete JSON.
    bool tryParseReadBuffer();

    std::unique_ptr<Message> createMessageFromJson(json j);

    // Flush write buffer in non-blocking manner; return bytes sent.
    ssize_t flushWriteBufferNonBlocking();

    // Set callback invoked when buffer becomes non-empty after append.
    void setWritableNotifier(std::function<void(int)> cb) { writable_notifier_ = std::move(cb); }
};

