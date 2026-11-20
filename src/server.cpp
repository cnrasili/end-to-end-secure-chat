#include "server.h"

#include <iostream>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace chat {

Server::Server(int port) : port_(port) {}

Server::~Server() {
    closeSocket(listenSocket_);

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : clients_) {
        closeSocket(entry.second);
    }
    clients_.clear();
}

bool Server::acceptConnections() {
    listenSocket_ = listenOn(port_);
    if (listenSocket_ == kInvalidSocket) {
        std::cerr << "[server] cannot listen on port " << port_ << "\n";
        return false;
    }

    std::cout << "[server] listening on port " << port_ << std::endl;

    while (true) {
#ifdef _WIN32
        SOCKET accepted = ::accept(static_cast<SOCKET>(listenSocket_), nullptr, nullptr);
#else
        int accepted = ::accept(static_cast<int>(listenSocket_), nullptr, nullptr);
#endif
        if (accepted == static_cast<decltype(accepted)>(kInvalidSocket)) {
            std::cerr << "[server] accept failed\n";
            break;
        }

        std::thread(&Server::handleClient, this,
                    static_cast<SocketHandle>(accepted)).detach();
    }

    return false;
}

bool Server::addClient(const std::string& username, SocketHandle socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (clients_.find(username) != clients_.end()) {
        return false;
    }
    clients_.emplace(username, socket);
    return true;
}

void Server::removeClient(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(username);
}

bool Server::forward(const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto target = clients_.find(msg.receiver);
    if (target == clients_.end()) {
        return false;
    }
    return sendPacket(target->second, msg);
}

bool Server::forwardToOthers(const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool delivered = false;
    for (const auto& entry : clients_) {
        if (entry.first == msg.sender) {
            continue;
        }
        if (sendPacket(entry.second, msg)) {
            delivered = true;
        }
    }
    return delivered;
}

void Server::handleClient(SocketHandle socket) {
    Message first;
    if (!receivePacket(socket, first) ||
        first.type != MessageType::Register ||
        first.sender.empty()) {
        std::cerr << "[server] a client did not register properly\n";
        closeSocket(socket);
        return;
    }

    const std::string username = first.sender;
    if (!addClient(username, socket)) {
        std::cerr << "[server] name already in use: " << username << "\n";
        closeSocket(socket);
        return;
    }

    std::cout << "[server] " << username << " connected" << std::endl;

    Message msg;
    while (receivePacket(socket, msg)) {
        // The payload stays encrypted; only the header fields are read here
        std::cout << "[server] " << messageTypeName(msg.type)
                  << " from " << msg.sender
                  << " (" << msg.payload.size() << " bytes) -> "
                  << (msg.receiver.empty() ? std::string("everyone else") : msg.receiver)
                  << std::endl;

        const bool delivered = msg.receiver.empty() ? forwardToOthers(msg) : forward(msg);
        if (!delivered) {
            std::cout << "[server] nobody to deliver to, packet dropped" << std::endl;
        }
    }

    std::cout << "[server] " << username << " disconnected" << std::endl;
    removeClient(username);
    closeSocket(socket);
}

}  // namespace chat
