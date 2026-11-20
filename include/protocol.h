#pragma once

#include <cstddef>
#include <cstdint>

namespace chat {

// Packet types used by the protocol
enum class MessageType : uint8_t {
    Register    = 1,
    PublicKey   = 2,
    KeyExchange = 3,
    Text        = 4,
    Voice       = 5,
    Image       = 6
};

// Fixed header: type, sender length, receiver length, payload length
constexpr size_t kHeaderSize = 7;

// Upper bound for one packet payload
constexpr uint32_t kMaxPayloadSize = 16u * 1024u * 1024u;

constexpr int kDefaultPort = 5555;

// AES-256-GCM parameters
constexpr size_t kAesKeySize = 32;
constexpr size_t kGcmIvSize  = 12;
constexpr size_t kGcmTagSize = 16;

// RSA key length used for the key exchange
constexpr int kRsaBits = 2048;

// Returns a readable name for a packet type
const char* messageTypeName(MessageType type);

}  // namespace chat
