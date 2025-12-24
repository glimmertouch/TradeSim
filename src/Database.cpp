#include "Database.h"
#include <iostream>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>

namespace {
std::string toHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

std::vector<unsigned char> fromHex(const std::string& hex) {
    std::vector<unsigned char> out;
    if (hex.size() % 2 != 0) return out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int byte;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> byte;
        out.push_back(static_cast<unsigned char>(byte));
    }
    return out;
}

std::string genSaltHex(size_t nbytes = 16) {
    std::vector<unsigned char> buf(nbytes);
    if (RAND_bytes(buf.data(), static_cast<int>(buf.size())) != 1) {
        // fallback to pseudo-random if OpenSSL fails
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<unsigned char>(rand() & 0xFF);
    }
    return toHex(buf.data(), buf.size());
}

std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt_hex, int iters = 100000, int dkLen = 32) {
    auto salt_bytes = fromHex(salt_hex);
    std::vector<unsigned char> out(dkLen);
    const EVP_MD* md = EVP_sha256();
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                           salt_bytes.data(), static_cast<int>(salt_bytes.size()),
                           iters, md, static_cast<int>(out.size()), out.data()) != 1) {
        return {};
    }
    return toHex(out.data(), out.size());
}
} // namespace

Database& Database::instance() {
    static Database inst;
    return inst;
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

bool Database::init(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "sqlite3_open error: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "pass_hash TEXT NOT NULL,"
        "salt TEXT NOT NULL,"
        "balance REAL DEFAULT 100000.0"
        ");";
    return execSql(create_sql);
}

bool Database::execSql(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : "(null)") << "\n";
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::createUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lk(mtx_);
    // generate salt and hash
    std::string salt_hex = genSaltHex();
    std::string hash_hex = pbkdf2_sha256_hex(password, salt_hex);
    if (hash_hex.empty()) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* q = "INSERT INTO users (username, pass_hash, salt) VALUES (?1, ?2, ?3);";
    if (sqlite3_prepare_v2(db_, q, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash_hex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, salt_hex.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        // 可能是 UNIQUE 约束失败等
        // std::cerr << "insert failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    return true;
}

std::optional<int> Database::verifyUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lk(mtx_);
    sqlite3_stmt* stmt = nullptr;
    const char* q = "SELECT id, pass_hash, salt FROM users WHERE username = ?1;";
    if (sqlite3_prepare_v2(db_, q, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* ph = sqlite3_column_text(stmt, 1);
    const unsigned char* sl = sqlite3_column_text(stmt, 2);
    std::string stored_hash = ph ? reinterpret_cast<const char*>(ph) : "";
    std::string salt_hex = sl ? reinterpret_cast<const char*>(sl) : "";
    sqlite3_finalize(stmt);

    std::string calc = pbkdf2_sha256_hex(password, salt_hex);
    if (!calc.empty() && calc == stored_hash) return id;
    return std::nullopt;
}
