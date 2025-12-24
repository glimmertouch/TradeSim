#include "Database.h"
#include <iostream>

int main() {
    if (!Database::instance().init("trade.db")) {
        std::cerr << "DB init failed\n"; return 1;
    }
    std::string cmd;
    while (true) {
        std::cout << "[cmd] create <user> <pwd>  |  check <user> <pwd>  |  quit\n> ";
        if (!(std::cin >> cmd)) break;
        if (cmd == "create") {
            std::string u,p; std::cin >> u >> p;
            bool ok = Database::instance().createUser(u,p);
            std::cout << (ok ? "created\n" : "create failed (maybe exists)\n");
        } else if (cmd == "check") {
            std::string u,p; std::cin >> u >> p;
            auto id = Database::instance().verifyUser(u,p);
            if (id) std::cout << "ok uid=" << *id << "\n"; else std::cout << "invalid\n";
        } else if (cmd == "quit") break;
    }
    return 0;
}