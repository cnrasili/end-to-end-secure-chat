#include "message.h"

#include <utility>

namespace chat {

const char* messageTypeName(MessageType type) {
    switch (type) {
        case MessageType::Register:    return "REGISTER";
        case MessageType::PublicKey:   return "PUBLIC_KEY";
        case MessageType::KeyExchange: return "KEY_EXCHANGE";
        case MessageType::Text:        return "TEXT";
        case MessageType::Voice:       return "VOICE";
        case MessageType::Image:       return "IMAGE";
    }
    return "UNKNOWN";
}

namespace {

// Checks that a byte value belongs to one of the defined packet types
bool isKnownType(uint8_t raw) {
    return raw >= static_cast<uint8_t>(MessageType::Register) &&
           raw <= static_cast<uint8_t>(MessageType::Image);
}

}  // namespace

Message::Message(MessageType t, std::string from, std::string to, Bytes data)
    : type(t), sender(std::move(from)), receiver(std::move(to)), payload(std::move(data)) {}

Bytes Message::serialize() const {
    const uint8_t senderLen   = static_cast<uint8_t>(sender.size());
    const uint8_t receiverLen = static_cast<uint8_t>(receiver.size());
    const uint32_t payloadLen = static_cast<uint32_t>(payload.size());

    Bytes out;
    out.reserve(kHeaderSize + senderLen + receiverLen + payload.size());

    out.push_back(static_cast<uint8_t>(type));
    out.push_back(senderLen);
    out.push_back(receiverLen);

    // Length field in network byte order
    out.push_back(static_cast<uint8_t>((payloadLen >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((payloadLen >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(payloadLen & 0xFF));

    out.insert(out.end(), sender.begin(), sender.end());
    out.insert(out.end(), receiver.begin(), receiver.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool Message::parseHeader(const uint8_t* header,
                          MessageType& type,
                          uint8_t& senderLen,
                          uint8_t& receiverLen,
                          uint32_t& payloadLen) {
    if (header == nullptr || !isKnownType(header[0])) {
        return false;
    }

    type        = static_cast<MessageType>(header[0]);
    senderLen   = header[1];
    receiverLen = header[2];
    payloadLen  = (static_cast<uint32_t>(header[3]) << 24) |
                  (static_cast<uint32_t>(header[4]) << 16) |
                  (static_cast<uint32_t>(header[5]) << 8) |
                  static_cast<uint32_t>(header[6]);

    return payloadLen <= kMaxPayloadSize;
}

bool Message::deserialize(const Bytes& buffer, Message& out) {
    if (buffer.size() < kHeaderSize) {
        return false;
    }

    MessageType type{};
    uint8_t senderLen = 0;
    uint8_t receiverLen = 0;
    uint32_t payloadLen = 0;
    if (!parseHeader(buffer.data(), type, senderLen, receiverLen, payloadLen)) {
        return false;
    }

    const size_t expected = kHeaderSize + senderLen + receiverLen + payloadLen;
    if (buffer.size() != expected) {
        return false;
    }

    const uint8_t* cursor = buffer.data() + kHeaderSize;
    out.type = type;
    out.sender.assign(cursor, cursor + senderLen);
    cursor += senderLen;
    out.receiver.assign(cursor, cursor + receiverLen);
    cursor += receiverLen;
    out.payload.assign(cursor, cursor + payloadLen);
    return true;
}

}  // namespace chat
