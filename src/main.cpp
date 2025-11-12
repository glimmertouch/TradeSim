// Refactored entry point using TradeServer abstraction
#include "TradeServer.h"
#include "MarketDataGenerator.h"
#include <iostream>
#include <memory>

int main() {
    TradeServer server(8000);
    server.setMarketDataGenerator(std::make_unique<MarketDataGenerator>());
    if (!server.init()) {
        std::cerr << "Failed to init TradeServer" << std::endl;
        return 1;
    }
    server.run();
    return 0;
}
