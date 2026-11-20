#pragma once

#include <map>
#include <mutex>
#include <string>

#include "net.h"

namespace chat {

// Relay server: knows who is connected and forwards packets.
// It never holds the AES key, so it cannot read any message.
class Server {
public:
    explicit Server(int port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Opens the listening socket and serves clients until stopped
    bool acceptConnections();

private:
    // Runs in its own thread for every connected client
    void handleClient(SocketHandle socket);

    // Sends a packet to the user named in the receiver field
    bool forward(const Message& msg);

    // Sends a packet to every connected user except the sender
    bool forwardToOthers(const Message& msg);

    bool addClient(const std::string& username, SocketHandle socket);
    void removeClient(const std::string& username);

    int port_;
    SocketHandle listenSocket_ = kInvalidSocket;
    std::map<std::string, SocketHandle> clients_;
    std::mutex mutex_;
};

}  // namespace chat
