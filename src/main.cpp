// Refactored entry point using TradeServer abstraction
#include "TradeServer.h"
#include "Database.h"
#include <iostream>
#include <memory>

int main() {
    if (!Database::instance().init("trade.db")) {
        std::cerr << "DB init failed\n";
        return 1;
    }
    TradeServer server(8000);
    if (!server.init()) {
        std::cerr << "Failed to init TradeServer" << std::endl;
        return 1;
    }
    server.run();
    return 0;
}
