#include "net.h"

#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace chat {

namespace {

#ifdef _WIN32
using RawSocket = SOCKET;
#else
using RawSocket = int;
#endif

RawSocket raw(SocketHandle socket) {
    return static_cast<RawSocket>(socket);
}

}  // namespace

bool netStartup() {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void netCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void shutdownSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    ::shutdown(raw(socket), SD_BOTH);
#else
    ::shutdown(raw(socket), SHUT_RDWR);
#endif
}

void closeSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(raw(socket));
#else
    close(raw(socket));
#endif
}

SocketHandle connectTo(const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &results) != 0) {
        return kInvalidSocket;
    }

    SocketHandle handle = kInvalidSocket;
    for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        RawSocket sock = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (sock == static_cast<RawSocket>(kInvalidSocket)) {
            continue;
        }
        if (::connect(sock, entry->ai_addr, static_cast<int>(entry->ai_addrlen)) == 0) {
            handle = static_cast<SocketHandle>(sock);
            break;
        }
        closeSocket(static_cast<SocketHandle>(sock));
    }

    freeaddrinfo(results);

    if (handle != kInvalidSocket) {
        int flag = 1;
        setsockopt(raw(handle), IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag));
    }
    return handle;
}

SocketHandle listenOn(int port) {
    RawSocket sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == static_cast<RawSocket>(kInvalidSocket)) {
        return kInvalidSocket;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<unsigned short>(port));

    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(sock, 8) != 0) {
        closeSocket(static_cast<SocketHandle>(sock));
        return kInvalidSocket;
    }
    return static_cast<SocketHandle>(sock);
}

bool sendAll(SocketHandle socket, const uint8_t* data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        const int chunk = static_cast<int>(
            length - sent > 65536 ? 65536 : length - sent);
        const int written = ::send(raw(socket),
                                   reinterpret_cast<const char*>(data + sent),
                                   chunk, 0);
        if (written <= 0) {
            return false;
        }
        sent += static_cast<size_t>(written);
    }
    return true;
}

bool recvAll(SocketHandle socket, uint8_t* data, size_t length) {
    size_t read = 0;
    while (read < length) {
        const int chunk = static_cast<int>(
            length - read > 65536 ? 65536 : length - read);
        const int got = ::recv(raw(socket),
                               reinterpret_cast<char*>(data + read),
                               chunk, 0);
        if (got <= 0) {
            return false;
        }
        read += static_cast<size_t>(got);
    }
    return true;
}

bool sendPacket(SocketHandle socket, const Message& msg) {
    const Bytes packet = msg.serialize();
    return sendAll(socket, packet.data(), packet.size());
}

bool receivePacket(SocketHandle socket, Message& out) {
    uint8_t header[kHeaderSize];
    if (!recvAll(socket, header, kHeaderSize)) {
        return false;
    }

    MessageType type{};
    uint8_t senderLen = 0;
    uint8_t receiverLen = 0;
    uint32_t payloadLen = 0;
    if (!Message::parseHeader(header, type, senderLen, receiverLen, payloadLen)) {
        return false;
    }

    // The length field tells the receiver exactly where this packet ends
    Bytes buffer(kHeaderSize + senderLen + receiverLen + payloadLen);
    std::memcpy(buffer.data(), header, kHeaderSize);
    if (buffer.size() > kHeaderSize &&
        !recvAll(socket, buffer.data() + kHeaderSize, buffer.size() - kHeaderSize)) {
        return false;
    }

    return Message::deserialize(buffer, out);
}

}  // namespace chat
