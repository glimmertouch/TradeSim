#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <sqlite3.h>

class Database {
public:
    static Database& instance();
    ~Database();

    bool init(const std::string& path = "trade.db");

    bool createUser(const std::string& username, const std::string& password);

    std::optional<int> verifyUser(const std::string& username, const std::string& password);

private:
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool execSql(const std::string& sql);

    sqlite3* db_ = nullptr;
    std::mutex mtx_;
};
