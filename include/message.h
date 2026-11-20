#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"

namespace chat {

using Bytes = std::vector<uint8_t>;

// One packet travelling over the TCP connection
class Message {
public:
    MessageType type = MessageType::Text;
    std::string sender;
    std::string receiver;
    Bytes payload;

    Message() = default;
    Message(MessageType t, std::string from, std::string to, Bytes data);

    // Builds the full packet: header followed by sender, receiver and payload
    Bytes serialize() const;

    // Rebuilds a packet from a complete buffer
    static bool deserialize(const Bytes& buffer, Message& out);

    // Reads the fixed header so the receiver knows how many bytes follow
    static bool parseHeader(const uint8_t* header,
                            MessageType& type,
                            uint8_t& senderLen,
                            uint8_t& receiverLen,
                            uint32_t& payloadLen);
};

}  // namespace chat
