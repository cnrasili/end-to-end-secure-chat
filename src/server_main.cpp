#include <cstdlib>
#include <iostream>

#include "server.h"

int main(int argc, char** argv) {
    int port = chat::kDefaultPort;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "usage: chat_server [port]\n";
            return 1;
        }
    }

    if (!chat::netStartup()) {
        std::cerr << "[server] socket library failed to start\n";
        return 1;
    }

    chat::Server server(port);
    const bool ok = server.acceptConnections();

    chat::netCleanup();
    return ok ? 0 : 1;
}
