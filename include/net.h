#pragma once

#include <string>

#include "message.h"

namespace chat {

#ifdef _WIN32
using SocketHandle = unsigned long long;
constexpr SocketHandle kInvalidSocket = ~0ULL;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = static_cast<SocketHandle>(-1);
#endif

// Starts and stops the socket library (needed on Windows)
bool netStartup();
void netCleanup();

void closeSocket(SocketHandle socket);

// Opens a TCP connection to the relay server
SocketHandle connectTo(const std::string& host, int port);

// Opens the listening socket of the relay server
SocketHandle listenOn(int port);

// Sends and receives exactly the requested number of bytes
bool sendAll(SocketHandle socket, const uint8_t* data, size_t length);
bool recvAll(SocketHandle socket, uint8_t* data, size_t length);

// Writes one complete packet
bool sendPacket(SocketHandle socket, const Message& msg);

// Reads the header first and then the announced number of bytes
bool receivePacket(SocketHandle socket, Message& out);

}  // namespace chat
